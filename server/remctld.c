/*
 * The remctl server.
 *
 * Handles option parsing, network setup, and the basic processing loop of the
 * remctld server.  Supports either being run from inetd or tcpserver or
 * running as a stand-alone daemon and managing its own network connections.
 *
 * Written by Anton Ushakov
 * Extensive modifications by Russ Allbery <eagle@eyrie.org>
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2010, 2011, 2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/event.h>
#include <portable/gssapi.h>
#include <portable/sd-daemon.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <signal.h>
#include <syslog.h>
#include <sys/wait.h>
#include <time.h>

#include <server/internal.h>
#include <util/fdflag.h>
#include <util/macros.h>
#include <util/messages.h>
#include <util/network.h>
#include <util/vector.h>
#include <util/xmalloc.h>

/*
 * Flag indicating whether we've received a SIGCHLD and need to reap children
 * (only used in standalone mode).
 */
static volatile sig_atomic_t child_signaled = 0;

/*
 * Flag indicating whether we've received a signal asking us to re-read our
 * configuration file (only used in standalone mode).
 */
static volatile sig_atomic_t config_signaled = 0;

/*
 * Flag indicating whether we've received a signal asking us to exit (only
 * used in standalone mode).
 */
static volatile sig_atomic_t exit_signaled = 0;

/* Usage message. */
static const char usage_message[] = "\
Usage: remctld <options>\n\
\n\
Options:\n\
    -b <addr>     Bind to a specific address (may be given multiple times)\n\
    -d            Log verbose debugging information\n\
    -F            Run in the foreground instead of forking and exiting\n\
    -f <file>     Config file (default: " CONFIG_FILE ")\n\
    -h            Display this help\n\
    -m            Stand-alone daemon mode, meant mostly for testing\n\
    -P <file>     Write PID to file, only useful with -m\n\
    -p <port>     Port to use, only for standalone mode (default: 4373)\n\
    -S            Log to standard output/error rather than syslog\n\
    -s <service>  Service principal to use (default: host/<host>)\n\
    -v            Display the version of remctld\n\
    -Z            Raise SIGSTOP once ready for connections\n\
\n\
Supported ACL methods: file, princ, deny";

/* Structure used to store program options. */
struct options {
    bool debug;                 /* -d: log verbose debugging information */
    bool foreground;            /* -F: run in the foreground */
    bool log_stdout;            /* -S: log to standard output and error */
    bool standalone;            /* -m: run in stand-alone daemon mode */
    bool suspend;               /* -Z: raise SIGSTOP when ready */
    unsigned short port;        /* -p: port on which to listen */
    char *service;              /* -s: service principal to use */
    const char *config_path;    /* -f: path to the configuration file */
    const char *pid_path;       /* -P: path to the PID file to write */
    struct vector *bindaddrs;   /* -b: bind to a specific address */
};


/*
 * Display the usage message for remctld.
 */
static void
usage(int status)
{
    FILE *output;

    output = (status == 0) ? stdout : stderr;
    if (status != 0)
        fprintf(output, "\n");
    fprintf(output, usage_message);
#ifdef HAVE_GPUT
    fprintf(output, ", gput");
#endif
#ifdef HAVE_PCRE
    fprintf(output, ", pcre");
#endif
#ifdef HAVE_REGCOMP
    fprintf(output, ", regex");
#endif
#ifdef HAVE_REMCTL_UNXGRP_ACL
    fprintf(output, ", unxgrp");
#endif
    fprintf(output, "\n");
    exit(status);
}


/*
 * Signal handler for child processes forked when running in standalone mode.
 * Just set the child_signaled global so that we know to reap the processes
 * later.
 */
static RETSIGTYPE
child_handler(int sig UNUSED)
{
    child_signaled = 1;
}


/*
 * Signal handler for signals asking us to re-read our configuration file when
 * running in standalone mode.  Set the config_signaled global so that we do
 * this the next time through the processing loop.
 */
static RETSIGTYPE
config_handler(int sig UNUSED)
{
    config_signaled = 1;
}


/*
 * Signal handler for signals asking for a clean shutdown when running in
 * standalone mode.  Set the exit_signaled global so that we exit cleanly the
 * next time through the processing loop.
 */
static RETSIGTYPE
exit_handler(int sig UNUSED)
{
    exit_signaled = 1;
}


/*
 * The logging callback for libevent.  We hook this into our message system so
 * that libevent messages are handled the same way as our other internal
 * messages.  This function should be passed to event_set_log_callback at the
 * start of libevent initialization.
 */
static void
event_log_callback(int severity, const char *message)
{
    switch (severity) {
    case EVENT_LOG_DEBUG:
        debug("%s", message);
        break;
    case EVENT_LOG_MSG:
        notice("%s", message);
        break;
    default:
        warn("%s", message);
        break;
    }
}


/*
 * The fatal callback for libevent.  Convert this to die, so that it's logged
 * the same as our other messages.  This function should be passed to
 * event_set_fatal_callback at the start of libevent initialization.
 */
static void
event_fatal_callback(int err)
{
    die("fatal libevent error (%d)", err);
}


/*
 * Given a service name, imports it and acquires credentials for it, storing
 * them in the second argument.  Returns true on success and false on failure,
 * logging an error message.
 *
 * Normally, you don't want to do this; instead, normally you want to allow
 * the underlying GSS-API library choose the appropriate credentials from a
 * keytab for each incoming connection.
 */
static bool
acquire_creds(char *service, gss_cred_id_t *creds)
{
    gss_buffer_desc buffer;
    gss_name_t name;
    OM_uint32 major, minor;

    buffer.value = service;
    buffer.length = strlen(buffer.value) + 1;
    major = gss_import_name(&minor, &buffer, GSS_C_NT_USER_NAME, &name);
    if (major != GSS_S_COMPLETE) {
        warn_gssapi("while importing name", major, minor);
        return false;
    }
    major = gss_acquire_cred(&minor, name, 0, GSS_C_NULL_OID_SET,
                             GSS_C_ACCEPT, creds, NULL, NULL);
    if (major != GSS_S_COMPLETE) {
        warn_gssapi("while acquiring credentials", major, minor);
        return false;
    }
    gss_release_name(&minor, &name);
    return true;
}


/*
 * Handle the interaction with the client.  Takes the client file descriptor,
 * the server configuration, and the server credentials.  Establishes a
 * security context, processes requests from the client, checks the ACL file
 * as appropriate, and then spawns commands, sending the output back to the
 * client.  This function only returns when the client connection has
 * completed, either successfully or unsuccessfully.
 */
static void
handle_connection(int fd, struct config *config, gss_cred_id_t creds)
{
    struct client *client;

    /* Establish a context with the client. */
    client = server_new_client(fd, creds);
    if (client == NULL) {
        close(fd);
        return;
    }
    debug("accepted connection from %s (protocol %d)", client->user,
          client->protocol);

    /*
     * Now, we process incoming commands.  This is handled differently
     * depending on the protocol version.  These functions won't exit until
     * the client is done sending commands and we're done replying.
     */
    if (client->protocol == 1)
        server_v1_handle_messages(client, config);
    else
        server_v2_handle_messages(client, config);

    /* We're done; shut down the client connection. */
    server_free_client(client);
}


/*
 * Gather information about an exited child and log an appropriate message.
 * We keep the log level to debug unless something interesting happened, like
 * a non-zero exit status.
 */
static void
log_child(pid_t pid, int status)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0)
            warn("child %lu exited with %d", (unsigned long) pid,
                 WEXITSTATUS(status));
        else
            debug("child %lu done", (unsigned long) pid);
    } else if (WIFSIGNALED(status)) {
        warn("child %lu died on signal %d", (unsigned long) pid,
             WTERMSIG(status));
    } else {
        warn("child %lu died", (unsigned long) pid);
    }
}


/*
 * Given a bind address, return true if it's an IPv6 address.  Otherwise, it's
 * assumed to be an IPv4 address.
 */
#ifdef HAVE_INET6
static bool
is_ipv6(const char *string)
{
    struct in6_addr addr;
    return inet_pton(AF_INET6, string, &addr) == 1;
}
#else
static bool
is_ipv6(const char *string UNUSED)
{
    return false;
}
#endif


/*
 * Bind the listening socket or sockets on which we accept requests and return
 * a list of sockets in the fds parameter.  Return a count of sockets in the
 * count parameter.
 *
 * Handle the socket activation case where the socket has already been set up
 * for us by systemd and, in that case, just return the already-configured
 * socket.
 */
static void
bind_sockets(struct options *options, socket_type **fds,
             unsigned int *count)
{
    int status;
    size_t i;
    const char *addr;
    socket_type fd;

    /* Check whether systemd has already bound the sockets. */
    status = sd_listen_fds(true);
    if (status < 0)
        die("using systemd-bound sockets failed: %s", strerror(-status));
    if (status > 0) {
        *fds = xcalloc(status, sizeof(socket_type));
        for (i = 0; i < (size_t) status; i++)
            (*fds)[i] = SD_LISTEN_FDS_START + i;
        *count = status;
        return;
    }

    /*
     * We have to do the work ourselves.  If there is no bind address, bind to
     * all local sockets, which will normally result in two file descriptors
     * on which to listen.
     */
    if (options->bindaddrs->count == 0) {
        if (!network_bind_all(SOCK_STREAM, options->port, fds, count))
            sysdie("cannot bind any sockets");
        for (i = 0; i < *count; i++)
            if (listen((*fds)[i], 5) < 0)
                sysdie("error listening on socket");
        return;
    }

    /*
     * Otherwise, we have to iterate through all the bind addresses, bind them
     * using the appropriate function, and listen on each.
     */
    *count = options->bindaddrs->count;
    *fds = xcalloc(*count, sizeof(socket_type));
    for (i = 0; i < options->bindaddrs->count; i++) {
        addr = options->bindaddrs->strings[i];
        if (is_ipv6(addr))
            fd = network_bind_ipv6(SOCK_STREAM, addr, options->port);
        else
            fd = network_bind_ipv4(SOCK_STREAM, addr, options->port);
        if (fd == INVALID_SOCKET)
            sysdie("cannot bind to address %s, port %hu", addr, options->port);
        if (listen(fd, 5) < 0)
            sysdie("error listening on socket");
        (*fds)[i] = fd;
    }
}


/*
 * Write a PID to a file.  This is done via atomic replacement so that the
 * file never exists with no content.  Note that there is no locking and no
 * verification that an existing PID has exited.
 */
static void
write_pidfile(pid_t pid, const char *path)
{
    char *template;
    FILE *pid_file;
    int fd;

    xasprintf(&template, "%s.XXXXXX", path);
    fd = mkstemp(template);
    if (fd < 0)
        sysdie("cannot create temporary PID file %s", template);
    pid_file = fdopen(fd, "w");
    if (pid_file == NULL)
        sysdie("cannot reopen temporary PID file %s", template);
    if (fprintf(pid_file, "%ld\n", (long) pid) < 0)
        sysdie("cannot write to temporary PID file %s", template);
    fclose(pid_file);
    if (rename(template, path) < 0)
        sysdie("cannot rename temporary PID file to %s", path);
    free(template);
}


/*
 * Run as a daemon.  This is the main dispatch loop, which listens for network
 * connections, forks a child to process each connection, and reaps the
 * children when they're done.  This is only used in standalone mode; when run
 * from inetd or tcpserver, remctld processes one connection and then exits.
 */
static void
server_daemon(struct options *options, struct config *config,
              gss_cred_id_t creds)
{
    socket_type s;
    unsigned int nfds, i;
    socket_type *fds;
    pid_t child;
    int status;
    struct sigaction sa, oldsa;
    struct sockaddr_storage ss;
    socklen_t sslen;
    char ip[INET6_ADDRSTRLEN];
    OM_uint32 minor;

    /* Set up a SIGCHLD handler so that we know when to reap children. */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = child_handler;
    if (sigaction(SIGCHLD, &sa, &oldsa) < 0)
        sysdie("cannot set SIGCHLD handler");

    /* Set up exit handlers for signals that call for a clean shutdown. */
    sa.sa_handler = exit_handler;
    if (sigaction(SIGINT, &sa, NULL) < 0)
        sysdie("cannot set SIGINT handler");
    if (sigaction(SIGTERM, &sa, NULL) < 0)
        sysdie("cannot set SIGTERM handler");

    /* Set up a SIGHUP handler so that we know when to re-read our config. */
    sa.sa_handler = config_handler;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        sysdie("cannot set SIGHUP handler");

    /* Bind to the network sockets and configure listening addresses. */
    bind_sockets(options, &fds, &nfds);

    /*
     * Set up our PID file now that we're ready to accept connections, so that
     * the PID file isn't created until clients can connect.
     */
    if (options->pid_path != NULL)
        write_pidfile(getpid(), options->pid_path);

    /* Log a starting message. */
    notice("starting");

    /* Indicate to systemd that we're ready to answer requests. */
    status = sd_notify(true, "READY=1");
    if (status < 0)
        warn("cannot notify systemd of startup: %s", strerror(-status));

    /* Indicate to upstart that we're ready to answer requests. */
    if (options->suspend)
        if (raise(SIGSTOP) < 0)
            syswarn("cannot notify upstart of startup");

    /*
     * The main processing loop.  Each time through the loop, check to see if
     * we need to reap children, check to see if we should re-read our
     * configuration, and check to see if we're exiting.  Then see if we have
     * a new connection, and if so, fork a child to handle it.
     *
     * Note that there are no limits here on the number of simultaneous
     * processes, so you may want to set system resource limits to prevent an
     * attacker from consuming all available processes.
     */
    while (1) {
        if (child_signaled) {
            child_signaled = 0;
            while ((child = waitpid(0, &status, WNOHANG)) > 0)
                log_child(child, status);
            if (child < 0 && errno != ECHILD)
                sysdie("waitpid failed");
        }
        if (config_signaled) {
            config_signaled = 0;
            notice("re-reading configuration");
            server_config_free(config);
            config = server_config_load(options->config_path);
            if (config == NULL)
                die("cannot load configuration file %s", options->config_path);
        }
        if (exit_signaled) {
            notice("signal received, exiting");
            break;
        }
        sslen = sizeof(ss);
        s = network_accept_any(fds, nfds, (struct sockaddr *) &ss, &sslen);
        if (s == INVALID_SOCKET) {
            if (errno != EINTR)
                sysdie("error accepting incoming connection");
            continue;
        }
        fdflag_close_exec(s, true);
        child = fork();
        if (child < 0) {
            syswarn("forking a new child failed");
            warn("sleeping ten seconds in the hope we recover...");
            sleep(10);
        } else if (child == 0) {
            for (i = 0; i < nfds; i++)
                close(fds[i]);
            network_bind_all_free(fds);
            if (sigaction(SIGCHLD, &oldsa, NULL) < 0)
                syswarn("cannot reset SIGCHLD handler");
            handle_connection(s, config, creds);
            if (creds != GSS_C_NO_CREDENTIAL)
                gss_release_cred(&minor, &creds);
            if (options->log_stdout)
                fflush(stdout);
            server_config_free(config);
            vector_free(options->bindaddrs);
            libevent_global_shutdown();
            message_handlers_reset();
            exit(0);
        } else {
            close(s);
            network_sockaddr_sprint(ip, sizeof(ip), (struct sockaddr *) &ss);
            debug("child %lu for %s", (unsigned long) child, ip);
        }
    }

    /*
     * Clean up resources at the end of the loop.  This is not strictly
     * necessary, but it helps valgrind testing.
     */
    if (options->pid_path != NULL)
        unlink(options->pid_path);
    for (i = 0; i < nfds; i++)
        close(fds[i]);
    network_bind_all_free(fds);
}


/*
 * Main routine.  Parses command-line arguments, determines whether we're
 * running in stand-alone or inetd mode, and does the connection handling if
 * running in standalone mode.  User connections are handed off to
 * process_connection.
 */
int
main(int argc, char *argv[])
{
    struct options options;
    int option;
    struct sigaction sa;
    gss_cred_id_t creds = GSS_C_NO_CREDENTIAL;
    OM_uint32 minor;
    struct config *config;

    /* Ignore SIGPIPE errors from our children. */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) < 0)
        sysdie("cannot set SIGPIPE handler");

    /* Establish identity for logging. */
    message_program_name = "remctld";

    /* Initialize the logging and fatal callbacks for libevent. */
    event_set_log_callback(event_log_callback);
    event_set_fatal_callback(event_fatal_callback);

    /* Initialize options. */
    memset(&options, 0, sizeof(options));
    options.port = 4373;
    options.config_path = CONFIG_FILE;
    options.bindaddrs = vector_new();

    /* Parse options. */
    while ((option = getopt(argc, argv, "b:dFf:hk:mP:p:Ss:vZ")) != EOF) {
        switch (option) {
        case 'b':
            vector_add(options.bindaddrs, optarg);
            break;
        case 'd':
            options.debug = true;
            break;
        case 'F':
            options.foreground = true;
            break;
        case 'f':
            options.config_path = optarg;
            break;
        case 'h':
            usage(0);
            break;
        case 'k':
            if (setenv("KRB5_KTNAME", optarg, 1) < 0)
                sysdie("cannot set KRB5_KTNAME");
            break;
        case 'm':
            options.standalone = true;
            break;
        case 'P':
            options.pid_path = optarg;
            break;
        case 'p':
            options.port = atoi(optarg);
            break;
        case 'S':
            options.log_stdout = true;
            break;
        case 's':
            options.service = optarg;
            break;
        case 'v':
            printf("remctld %s\n", PACKAGE_VERSION);
            exit(0);
            break;
        case 'Z':
            options.suspend = true;
            break;
        default:
            usage(1);
            break;
        }
    }

    /* Check arguments for consistency. */
    if (options.bindaddrs->count > 0 && !options.standalone)
        die("-b only makes sense in combination with -m");
    if (options.suspend && !options.standalone)
        die("-Z only makes sense in combination with -m");

    /* Daemonize if told to do so. */
    if (options.standalone && !options.foreground)
        if (daemon(0, options.log_stdout) != 0)
            sysdie("cannot daemonize");

    /*
     * Set up syslog unless stdout/stderr was requested.  Set up debug logging
     * if requested.
     */
    if (options.log_stdout) {
        if (options.debug)
            message_handlers_debug(1, message_log_stdout);
    } else {
        openlog("remctld", LOG_PID | LOG_NDELAY, LOG_DAEMON);
        message_handlers_notice(1, message_log_syslog_info);
        message_handlers_warn(1, message_log_syslog_warning);
        message_handlers_die(1, message_log_syslog_err);
        if (options.debug)
            message_handlers_debug(1, message_log_syslog_debug);
    }

    /* Read the configuration file. */
    config = server_config_load(options.config_path);
    if (config == NULL)
        die("cannot read configuration file %s", options.config_path);

    /*
     * If a service was specified, we should load only those credentials since
     * those are the only ones we're allowed to use.  Otherwise, creds will
     * keep its default value of GSS_C_NO_CREDENTIAL, which means support
     * anything that's in the keytab.
     */
    if (options.service != NULL)
        if (!acquire_creds(options.service, &creds))
            die("unable to acquire creds, aborting");

    /*
     * If we're not running as a daemon, just process the connection.
     * Otherwise, create a socket and listen on the socket, processing each
     * incoming connection.
     */
    if (!options.standalone)
        handle_connection(STDIN_FILENO, config, creds);
    else
        server_daemon(&options, config, creds);

    /* Clean up and exit. */
    server_config_free(config);
    if (creds != GSS_C_NO_CREDENTIAL)
        gss_release_cred(&minor, &creds);
    vector_free(options.bindaddrs);
    libevent_global_shutdown();
    message_handlers_reset();
    return 0;
}

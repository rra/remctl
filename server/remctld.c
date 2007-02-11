/*  $Id$
**
**  The deamon for a service for remote execution of predefined commands.
**  Access is authenticated via GSS-API Kerberos 5, authorized via ACL files.
**  Runs as a inetd/tcpserver deamon or a standalone program.
**
**  Written by Anton Ushakov
**  Extensive modifications by Russ Allbery <rra@stanford.edu>
**  Copyright 2002, 2003, 2004, 2005, 2006, 2007
**      Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>
#include <portable/gssapi.h>
#include <portable/socket.h>

#include <syslog.h>
#include <time.h>

#include <server/internal.h>
#include <util/util.h>

/* Usage message. */
static const char usage_message[] = "\
Usage: remctld <options>\n\
\n\
Options:\n\
    -d            Log verbose debugging information\n\
    -f <file>     Config file (default: " CONFIG_FILE ")\n\
    -h            Display this help\n\
    -m            Stand-alone daemon mode, meant mostly for testing\n\
    -P <file>     Write PID to file, only useful with -m\n\
    -p <port>     Port to use, only for standalone mode (default: 4444)\n\
    -S            Log to standard output/error rather than syslog\n\
    -s <service>  Service principal to use (default: host/<host>)\n\
    -v            Display the version of remctld\n";


/*
**  Display the usage message for remctld.
*/
static void
usage(int status)
{
    fprintf((status == 0) ? stdout : stderr, usage_message);
    if (status == 0)
        exit(0);
    else
        die("invalid usage");
}


/*
**  Given a service name, imports it and acquires credentials for it, storing
**  them in the second argument.  Returns true on success and false on
**  failure, logging an error message.
**
**  Normally, you don't want to do this; instead, normally you want to allow
**  the underlying GSS-API library choose the appropriate credentials from a
**  keytab for each incoming connection.
*/
static int
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
        return 0;
    }
    major = gss_acquire_cred(&minor, name, 0, GSS_C_NULL_OID_SET,
                             GSS_C_ACCEPT, creds, NULL, NULL);
    if (major != GSS_S_COMPLETE) {
        warn_gssapi("while acquiring credentials", major, minor);
        return 0;
    }
    gss_release_name(&minor, &name);
    return 1;
}


/*
**  Handle the interaction with the client.  Takes the client file descriptor,
**  the server configuration, and the server credentials.  Establishes a
**  security context, processes requests from the client, checks the ACL file
**  as appropriate, and then spawns commands, sending the output back to the
**  client.  This function only returns when the client connection has
**  completed, either successfully or unsuccessfully.
*/
static void
server_handle_connection(int fd, struct config *config, gss_cred_id_t creds)
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

    /* Now, we process incoming commands.  This is handled differently
       depending on the protocol version.  These functions won't exit until
       the client is done sending commands and we're done replying. */
    if (client->protocol == 1)
        server_v1_handle_commands(client, config);
    else
        server_v2_handle_commands(client, config);

    /* We're done; shut down the client connection. */
    server_free_client(client);
}


/*
**  Main routine.  Parses command-line arguments, determines whether we're
**  running in stand-alone or inetd mode, and does the connection handling if
**  running in standalone mode.  User connections are handed off to
**  process_connection.
*/
int
main(int argc, char *argv[])
{
    char *service = NULL;
    const char *pid_path = NULL;
    FILE *pid_file;
    int option;
    gss_cred_id_t creds = GSS_C_NO_CREDENTIAL;
    OM_uint32 minor;
    unsigned short port = 4444;
    int s, stmp;
    int do_standalone = 0;
    int do_stdout = 0;
    int do_debug = 0;
    const char *conffile = CONFIG_FILE;
    struct config *config;

    /* Since we are normally called from tcpserver or inetd, prevent clients
       from holding on to us forever by dying after an hour. */
    alarm(60 * 60);

    /* Establish identity. */
    message_program_name = "remctld";

    /* Parse options. */
    while ((option = getopt(argc, argv, "df:hmP:p:Ss:v")) != EOF) {
        switch (option) {
        case 'd':
            do_debug = 1;
            break;
        case 'f':
            conffile = optarg;
            break;
        case 'h':
            usage(0);
            break;
        case 'm':
            do_standalone = 1;
            break;
        case 'P':
            pid_path = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'S':
            do_stdout = 1;
            break;
        case 's':
            service = optarg;
            break;
        case 'v':
            printf("remctld %s\n", PACKAGE_VERSION);
            exit(0);
            break;
        default:
            usage(1);
            break;
        }
    }

    /* Set up syslog unless stdout/stderr was requested.  Set up debug logging
       if requestsed. */
    if (do_stdout) {
        if (do_debug)
            message_handlers_debug(1, message_log_stdout);
    } else {
        openlog("remctld", LOG_PID | LOG_NDELAY, LOG_DAEMON);
        message_handlers_notice(1, message_log_syslog_info);
        message_handlers_warn(1, message_log_syslog_warning);
        message_handlers_die(1, message_log_syslog_err);
        if (do_debug)
            message_handlers_debug(1, message_log_syslog_debug);
    }

    /* Read the configuration file. */
    config = server_config_load(conffile);
    if (config == NULL)
        die("cannot read configuration file %s", conffile);

    /* If a service was specified, we should load only those credentials since
       those are the only ones we're allowed to use.  Otherwise, creds will
       keep its default value of GSS_C_NO_CREDENTIAL, which means support
       anything that's in the keytab. */
    if (service != NULL) {
        if (!acquire_creds(service, &creds))
            die("unable to acquire creds, aborting");
    }

    /* If we're not running as a daemon, just process the connection.
       Otherwise, create a socket and listen on the socket, processing each
       incoming connection. */
    if (!do_standalone) {
        server_handle_connection(0, config, creds);
    } else {
        alarm(0);
        stmp = network_bind_ipv4("any", port);
        if (stmp < 0)
            sysdie("cannot create socket");
        if (listen(stmp, 5) < 0)
            sysdie("error listening on socket");
        if (pid_path != NULL) {
            pid_file = fopen(pid_path, "w");
            if (pid_file == NULL)
                sysdie("cannot create PID file %s", pid_path);
            fprintf(pid_file, "%ld\n", (long) getpid());
            fclose(pid_file);
        }
        do {
            s = accept(stmp, NULL, 0);
            if (s < 0) {
                syswarn("error accepting connection");
                continue;
            }
            server_handle_connection(s, config, creds);
            if (do_stdout)
                fflush(stdout);
        } while (1);
    }

    /* Clean up and exit.  We only reach here in regular mode. */
    if (creds != GSS_C_NO_CREDENTIAL)
        gss_release_cred(&minor, &creds);
    return 0;
}


/*
**  Local variables:
**  mode: c
**  c-basic-offset: 4
**  indent-tabs-mode: nil
**  end:
*/

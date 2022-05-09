/*
 * The remctl server as a restricted shell.
 *
 * This is a varient of remctld that, rather than listening to the network for
 * the remctl protocol, runs as a restricted shell under ssh.  It uses the
 * same configuration and the same command semantics as the normal remctl
 * server, but uses ssh as the transport mechanism and must be run under sshd.
 *
 * This file handles parsing of the user's command and the main control flow.
 *
 * Written by Russ Allbery
 * Copyright 2016, 2018, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/event.h>
#include <portable/system.h>

#include <signal.h>
#include <syslog.h>

#include <server/internal.h>
#include <util/messages.h>
#include <util/xmalloc.h>

/* Usage message. */
static const char usage_message[] = "\
Usage: remctl-shell [-dhqSv] [-f <file>] -c <command>\n\
       remctl-shell [-dqS] [-f <file>] <user>\n\
\n\
Options:\n\
    -c <command>  Specifies the command to run\n\
    -d            Log verbose debugging information\n\
    -f <file>     Config file (default: " CONFIG_FILE ")\n\
    -h            Display this help\n\
    -q            Suppress informational logging (such as the command run)\n\
    -S            Log to standard output/error rather than syslog\n\
    -v            Display the version of remctld\n\
\n\
This is meant to be used as the shell or forced command for a dedicated\n\
account, and handles incoming commands via ssh.  It must be run under ssh\n\
or with the same environment variables ssh would set.\n\
\n\
Supported ACL methods: file, princ, deny";


/*
 * Display the usage message for remctl-shell.
 */
__attribute__((__noreturn__)) static void
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
#if defined(HAVE_KRB5) && defined(HAVE_GETGRNAM_R)
    fprintf(output, ", localgroup");
#endif
#ifdef HAVE_PCRE
    fprintf(output, ", pcre");
#endif
#ifdef HAVE_REGCOMP
    fprintf(output, ", regex");
#endif
    fprintf(output, "\n");
    exit(status);
}


/*
 * Main routine.  Parses the configuration file and the user's command and
 * then dispatches running the command.
 */
int
main(int argc, char *argv[])
{
    int option, status;
    bool debug = false;
    bool log_stdout = false;
    bool quiet = false;
    struct sigaction sa;
    const char *command_string = NULL;
    const char *user = NULL;
    const char *config_path = CONFIG_FILE;
    struct iovec **command;
    struct client *client;
    struct config *config;

    /* Ignore SIGPIPE errors from our children. */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) < 0)
        sysdie("cannot set SIGPIPE handler");

    /* Establish identity for logging. */
    message_program_name = "remctl-shell";

    /* Initialize the logging and fatal callbacks for libevent. */
    event_set_log_callback(server_event_log_callback);
    event_set_fatal_callback(server_event_fatal_callback);

    /*
     * Parse options.  Since we're being run as a shell, there isn't all that
     * much here.
     */
    while ((option = getopt(argc, argv, "c:df:hqS")) != EOF) {
        switch (option) {
        case 'c':
            command_string = optarg;
            break;
        case 'd':
            debug = true;
            break;
        case 'f':
            config_path = optarg;
            break;
        case 'h':
            usage(0);
        case 'q':
            quiet = true;
            break;
        case 'S':
            log_stdout = true;
            break;
        case 'v':
            printf("remctl-shell %s\n", PACKAGE_VERSION);
            exit(0);
        default:
            warn("unknown option -%c", optopt);
            usage(1);
        }
    }
    argc -= optind;
    argv += optind;

    /*
     * If no command string was specified, the user must be specified as a
     * command-line argument and we'll get the command string from the
     * SSH_ORIGINAL_COMMAND environment variable.  If a command string was
     * specified, there must be no argument.
     */
    if (command_string == NULL) {
        if (argc != 1)
            usage(1);
        user = argv[0];
        command_string = getenv("SSH_ORIGINAL_COMMAND");
        if (command_string == NULL)
            die("SSH_ORIGINAL_COMMAND not set in the environment");
    } else {
        if (argc > 0)
            usage(1);
    }

    /* Set up logging. */
    if (log_stdout) {
        if (debug)
            message_handlers_debug(1, message_log_stdout);
    } else {
        openlog("remctl-shell", LOG_PID | LOG_NDELAY, LOG_DAEMON);
        message_handlers_notice(1, message_log_syslog_info);
        message_handlers_warn(1, message_log_syslog_warning);
        message_handlers_die(1, message_log_syslog_err);
        if (debug)
            message_handlers_debug(1, message_log_syslog_debug);
    }
    if (quiet)
        message_handlers_notice(0);

    /* Read the configuration file. */
    config = server_config_load(config_path);
    if (config == NULL)
        die("cannot read configuration file %s", config_path);

    /* Create the client struct based on the ssh environment. */
    client = server_ssh_new_client(user);

    /* Parse and execute the command. */
    command = server_ssh_parse_command(command_string);
    if (command == NULL)
        die("cannot parse command: %s", command_string);
    status = server_run_command(client, config, command);
    server_free_command(command);

    /* Clean up and exit. */
    server_ssh_free_client(client);
    server_config_free(config);
    libevent_global_shutdown();
    message_handlers_reset();
    return status;
}

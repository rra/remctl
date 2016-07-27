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
 * Note that, because there's no good way to pass command-line options into a
 * shell, it's not possible to reconfigure the configuration file path used by
 * remctl-shell.
 *
 * Written by Russ Allbery
 * Copyright 2016 Dropbox, Inc.
 *
 * See LICENSE for licensing terms.
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
Usage: remctl-shell [-dhS] -c <command>\n\
\n\
Options:\n\
    -c <command>  Specifies the command to run\n\
    -d            Log verbose debugging information\n\
    -h            Display this help\n\
    -S            Log to standard output/error rather than syslog\n\
\n\
This is meant to be used as the shell for a dedicated account and handles\n\
incoming commands via ssh.  It must be run under ssh or with the same\n\
environment variables ssh would set.\n";


/*
 * Display the usage message for remctl-shell.
 */
static void
usage(int status)
{
    FILE *output;

    output = (status == 0) ? stdout : stderr;
    if (status != 0)
        fprintf(output, "\n");
    fprintf(output, usage_message);
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
    struct sigaction sa;
    const char *command_string = NULL;
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
    while ((option = getopt(argc, argv, "dc:hS")) != EOF) {
        switch (option) {
        case 'c':
            command_string = optarg;
            break;
        case 'd':
            debug = true;
            break;
        case 'h':
            usage(0);
            break;
        case 'S':
            log_stdout = true;
            break;
        default:
            warn("unknown option -%c", optopt);
            usage(1);
            break;
        }
    }
    if (command_string == NULL)
        die("no command specified");

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

    /* Read the configuration file. */
    config = server_config_load(CONFIG_FILE);
    if (config == NULL)
        die("cannot read configuration file %s", CONFIG_FILE);

    /* Create the client struct based on the ssh environment. */
    client = server_ssh_new_client();

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

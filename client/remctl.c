/*
 * remctl command-line client.
 *
 * This is a command-line driver for the libremctl library, which takes the
 * command on the command line and prints out the results to standard output
 * and standard error as appropriate.
 *
 * Originally written by Anton Ushakov
 * Extensive modifications by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2002-2011, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/getopt.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <ctype.h>

#include <client/remctl.h>
#include <util/messages.h>
#include <util/xmalloc.h>

/* Usage message. */
static const char usage_message[] = "\
Usage: remctl <options> <host> <command> [<subcommand> [<parameters>]]\n\
\n\
Options:\n\
    -b <source>   Source IP used for outgoing connections\n\
    -d            Debugging level of output\n\
    -h            Display this help\n\
    -p <port>     remctld port (default: 4373 falling back to 4444)\n\
    -s <service>  remctld service principal (default: host/<host>)\n\
    -t <timeout>  Timeout in seconds (default: 0, disable timeout)\n\
    -v            Display the version of remctl\n";


/*
 * Display the usage message for remctl.
 */
__attribute__((__noreturn__)) static void
usage(int status)
{
    fprintf((status == 0) ? stdout : stderr, "%s", usage_message);
    exit(status);
}


/*
 * A wrapper around fwrite that checks the return status and reports an error
 * if the write failed.  This is probably futile for stderr, since reporting
 * the error will probably also fail, but we can try.
 */
static void
fwrite_checked(const void *data, size_t size, size_t nmemb, FILE *stream)
{
    if (fwrite(data, size, nmemb, stream) != nmemb)
        syswarn("local write of command output failed");
}


/*
 * Get the responses back from the server, taking appropriate action on each
 * one depending on its type.  Sets the errorcode parameter to the exit status
 * of the remote command, or to 1 if the remote command failed with an error.
 * Returns true on success, false if some protocol-level error occurred when
 * reading the responses.
 */
static bool
process_response(struct remctl *r, int *errorcode)
{
    struct remctl_output *out;

    *errorcode = 0;
    out = remctl_output(r);
    while (out != NULL && out->type != REMCTL_OUT_DONE) {
        switch (out->type) {
        case REMCTL_OUT_OUTPUT:
            if (out->stream == 1)
                fwrite_checked(out->data, out->length, 1, stdout);
            else if (out->stream == 2)
                fwrite_checked(out->data, out->length, 1, stderr);
            else {
                warn("unknown output stream %d", out->stream);
                fwrite_checked(out->data, out->length, 1, stderr);
            }
            break;
        case REMCTL_OUT_ERROR:
            *errorcode = 255;
            fwrite_checked(out->data, out->length, 1, stderr);
            fputc('\n', stderr);
            return true;
        case REMCTL_OUT_STATUS:
            *errorcode = out->status;
            return true;
        case REMCTL_OUT_DONE:
            break;
        }
        out = remctl_output(r);
    }
    if (out == NULL) {
        die("error reading from server: %s", remctl_error(r));
        return false;
    } else
        return true;
}


/*
 * Main routine.  Parse the arguments, open the remctl connection, send the
 * command, and then call process_response.
 */
int
main(int argc, char *argv[])
{
    int option, status;
    char *server_host;
    struct addrinfo hints, *ai;
    const char *source = NULL;
    const char *service_name = NULL;
    char *end;
    time_t timeout = 0;
    long tmp_port, tmp_timeout;
    unsigned short port = 0;
    struct remctl *r;
    int errorcode = 0;

    /* Set up logging and identity. */
    message_program_name = "remctl";
    if (!socket_init())
        die("failed to initialize socket library");

    /*
     * Parse options.  The + tells GNU getopt to stop option parsing at the
     * first non-argument rather than proceeding on to find options anywhere.
     * Without this, it's hard to call remote programs that take options.
     * Non-GNU getopt will treat the + as a supported option, which is handled
     * below.
     */
    while ((option = getopt(argc, argv, "+b:dhp:s:t:v")) != EOF) {
        switch (option) {
        case 'b':
            source = optarg;
            break;
        case 'd':
            message_handlers_debug(1, message_log_stderr);
            break;
        case 'h':
            usage(0);
        case 'p':
            tmp_port = strtol(optarg, &end, 10);
            if (*end != '\0' || tmp_port < 1 || tmp_port > (1L << 16) - 1)
                die("invalid port number %ld", tmp_port);
            port = (unsigned short) tmp_port;
            break;
        case 's':
            service_name = optarg;
            break;
        case 't':
            tmp_timeout = strtol(optarg, &end, 10);
            if (*end != '\0' || tmp_timeout < 0)
                die("invalid timeout value %ld", tmp_timeout);
            timeout = (time_t) tmp_timeout;
            break;
        case 'v':
            printf("%s\n", PACKAGE_STRING);
            exit(0);
        case '+':
            fprintf(stderr, "%s: invalid option -- +\n", argv[0]);
            usage(1);
        default:
            usage(1);
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 2)
        usage(1);
    server_host = *argv++;

    /*
     * If service_name isn't set, the remctl library uses host/<server>
     * (host@<server> in GSS-API parlance).  However, if the server to which
     * we're connecting is a DNS-load-balanced name, we have to be careful
     * what principal name we use.
     *
     * Ideally, we would let the GSS-API library handle this and choose
     * whether to canonicalize the <server> in the principal name based on the
     * krb5.conf rdns setting and similar configuration.  However, with DNS
     * load balancing, this still may fail.  At the time of network
     * connection, we will connect to whatever the name resolves to then.
     * After we connect, we authenticate, and the GSS-API library will then
     * separately canonicalize the hostname.  It could get a different answer
     * than we got for our network connection, leading to an authentication
     * failure.
     *
     * Therefore, if the principal isn't specified, we canonicalize the
     * hostname to which we're connecting before we connect.  Then, the
     * additional canonicalization possibly done by the GSS-API library should
     * return the same results and be consistent.
     *
     * Note that this opens the possibility of a subtle attack through DNS
     * spoofing, since both the principal used and the host to which we're
     * connecting can be changed by varying the DNS response.
     *
     * If the principal is specified explicitly, assume the user knows what
     * they're doing and don't do any of this.
     */
    if (service_name == NULL) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_CANONNAME;
        status = getaddrinfo(server_host, NULL, &hints, &ai);
        if (status != 0)
            die("cannot resolve host %s: %s", server_host,
                gai_strerror(status));
        server_host = xstrdup(ai->ai_canonname);
        freeaddrinfo(ai);
    }

    /* Open connection. */
    r = remctl_new();
    if (r == NULL)
        sysdie("cannot initialize remctl connection");

    if (timeout != 0)
        remctl_set_timeout(r, timeout);

    if (source != NULL)
        if (!remctl_set_source_ip(r, source))
            die("%s", remctl_error(r));
    if (!remctl_open(r, server_host, port, service_name))
        die("%s", remctl_error(r));

    /* Do the work. */
    if (!remctl_command(r, (const char **) argv))
        die("%s", remctl_error(r));
    if (!process_response(r, &errorcode))
        die("%s", remctl_error(r));

    /* Shut down cleanly. */
    remctl_close(r);
    socket_shutdown();
    return errorcode;
}

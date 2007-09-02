/*  $Id$
**
**  The client for a service for remote execution of predefined commands.
**  Access is authenticated via GSS-API Kerberos 5, authorized via ACL files.
**
**  Originally written by Anton Ushakov <antonu@stanford.edu>
**  Extensive modifications by Russ Allbery <rra@stanford.edu>
**  Copyright 2002, 2003, 2004, 2005, 2006, 2007
**      Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>
#include <portable/socket.h>

#include <ctype.h>

#include <client/remctl.h>
#include <util/util.h>

/* Usage message. */
static const char usage_message[] = "\
Usage: remctl <options> <host> <type> <service> <parameters>\n\
\n\
Options:\n\
    -d            Debugging level of output\n\
    -h            Display this help\n\
    -p <port>     remctld port (default: 4373 falling back to 4444)\n\
    -s <service>  remctld service principal (default: host/<host>)\n\
    -v            Display the version of remctl\n";


/*
**  Lowercase a string in place.
*/
static void 
lowercase(char *string)
{
    char *p;

    for (p = string; *p != '\0'; p++)
        *p = tolower(*p);
}


/*
**  Display the usage message for remctl.
*/
static void
usage(int status)
{
    fprintf((status == 0) ? stdout : stderr, "%s", usage_message);
    exit(status);
}


/*
**  Get the responses back from the server, taking appropriate action on each
**  one depending on its type.  Sets the errorcode parameter to the exit
**  status of the remote command, or to 1 if the remote command failed with an
**  error.  Returns true on success, false if some protocol-level error
**  occurred when reading the responses.
*/
static int
process_response(struct remctl *r, int *errorcode)
{
    struct remctl_output *out;

    *errorcode = 0;
    out = remctl_output(r);
    while (out != NULL && out->type != REMCTL_OUT_DONE) {
        switch (out->type) {
        case REMCTL_OUT_OUTPUT:
            if (out->stream == 1)
                fwrite(out->data, out->length, 1, stdout);
            else if (out->stream == 2)
                fwrite(out->data, out->length, 1, stderr);
            else {
                warn("unknown output stream %d", out->stream);
                fwrite(out->data, out->length, 1, stderr);
            }
            break;
        case REMCTL_OUT_ERROR:
            *errorcode = 255;
            fwrite(out->data, out->length, 1, stderr);
            fputc('\n', stderr);
            return 1;
        case REMCTL_OUT_STATUS:
            *errorcode = out->status;
            return 1;
        case REMCTL_OUT_DONE:
            break;
        }
        out = remctl_output(r);
    }
    if (out == NULL) {
        die("error reading from server: %s", remctl_error(r));
        return 0;
    } else
        return 1;
}


/*
**  Main routine.  Parse the arguments, open the remctl connection, send the
**  command, and then call process_response.
*/
int
main(int argc, char *argv[])
{
    int option, status;
    char *server_host;
    struct addrinfo hints, *ai;
    char *service_name = NULL;
    unsigned short port = 0;
    struct remctl *r;
    int errorcode = 0;

    /* Set up logging and identity. */
    message_program_name = "remctl";

    /* Parse options.  The + tells GNU getopt to stop option parsing at the
       first non-argument rather than proceeding on to find options anywhere.
       Without this, it's hard to call remote programs that take options.
       Non-GNU getopt will treat the + as a supported option, which is handled
       below. */
    while ((option = getopt(argc, argv, "+dhp:s:v")) != EOF) {
        switch (option) {
        case 'd':
            message_handlers_debug(1, message_log_stderr);
            break;
        case 'h':
            usage(0);
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 's':
            service_name = optarg;
            break;
        case 'v':
            printf("%s\n", PACKAGE_STRING);
            exit(0);
            break;
        case '+':
            fprintf(stderr, "%s: invalid option -- +\n", argv[0]);
        default:
            usage(1);
            break;
        }
    }
    argc -= optind;
    argv += optind;
    if (argc < 3)
        usage(1);
    server_host = *argv++;
    argc--;

    /*
     * If service_name isn't set, we use host/<server>.  However, if the
     * server to which we're connecting is a DNS-load-balanced name, we have
     * to be careful what principal name we use.  Canonicalize the name with
     * DNS (usually meaning a forward and reverse lookup).
     *
     * We then have to make sure that we connect to the same host that we're
     * using for the principal name.  If we get a different answer each time
     * we ask DNS (possible with DNS load balancing), we need to connect to
     * the canonical name rather than the original name given on the command
     * line.
     *
     * Note that this opens the possibility of a subtle attack through DNS
     * spoofing, since both the principal used and the host to which we're
     * connecting can be changed by varying the DNS response.  Providing a
     * service on the command-line will deactivate this behavior.
     */
    if (service_name == NULL) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_CANONNAME;
        status = getaddrinfo(server_host, NULL, &hints, &ai);
        if (status != 0)
            die("cannot resolve host %s: %s", server_host,
                gai_strerror(status));
        server_host = xstrdup(ai->ai_canonname);
        service_name = xmalloc(strlen("host/") + strlen(ai->ai_canonname) + 1);
        strcpy(service_name, "host/");
        strcat(service_name, ai->ai_canonname);
        freeaddrinfo(ai);
        lowercase(service_name);
    }

    /* Open connection. */
    r = remctl_new();
    if (r == NULL)
        sysdie("cannot initialize remctl connection");
    if (!remctl_open(r, server_host, port, service_name))
        die("%s", remctl_error(r));

    /* Do the work. */
    if (!remctl_command(r, (const char **) argv))
        die("%s", remctl_error(r));
    if (!process_response(r, &errorcode))
        die("%s", remctl_error(r));

    /* Shut down cleanly. */
    remctl_close(r);
    return errorcode;
}


/*
**  Local variables:
**  mode: c
**  c-basic-offset: 4
**  indent-tabs-mode: nil
**  end:
*/

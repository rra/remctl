/*  $Id$
**
**  The client for a "K5 sysctl" - a service for remote execution of 
**  predefined commands.  Access is authenticated via GSSAPI Kerberos 5, 
**  authorized via ACL files.
**
**  Written by Anton Ushakov <antonu@stanford.edu>
*/

#include <config.h>
#include <system.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include "gss-utils.h"
#include <util/util.h>

/* Handle compatibility to older versions of MIT Kerberos. */
#ifndef HAVE_GSS_RFC_OIDS
# define GSS_C_NT_USER_NAME gss_nt_user_name
#endif

/* Heimdal provides a nice #define for this. */
#if !HAVE_DECL_GSS_KRB5_MECHANISM
# include <gssapi/gssapi_krb5.h>
# define GSS_KRB5_MECHANISM gss_mech_krb5
#endif

/* Usage message. */
static const char usage_message[] = "\
Usage: remctl <options> <host> <type> <service> <parameters>\n\
\n\
Options:\n\
    -d            Debugging level of output\n\
    -h            Display this help\n\
    -p <port>     remctld port (default: 4444)\n\
    -s <service>  remctld service principal (default: host/<host>)\n\
    -v            Display the version of remctl\n";

/* These are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver. */
int READFD;
int WRITEFD;

/* Used in establishing context with gss server. */
gss_buffer_desc empty_token_buf = { 0, (void *) "" };
gss_buffer_t empty_token = &empty_token_buf;


/*
**  Lowercase a string in place.
*/
void 
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
**  Opens a TCP connection to the given hostname and port.  The host name is
**  resolved by gethostbyname and the socket is opened and connected.  Returns
**  the file descriptor or dies on failure.
*/
static int
connect_to_server(char *host, unsigned short port)
{
    struct sockaddr_in saddr;
    struct hostent *hp;
    int s;

    hp = gethostbyname(host);
    if (hp == NULL)
        die("unknown host: %s", host);

    saddr.sin_family = hp->h_addrtype;
    memcpy((char *)&saddr.sin_addr, hp->h_addr, sizeof(saddr.sin_addr));
    saddr.sin_port = htons(port);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        sysdie("error creating socket");
    if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
        sysdie("cannot connect to %s (port %hu)", host, port);

    return s;
}


/*
**  Given the service name of the remote server, establish a GSSAPI context
**  and return the context handle and any connection flags in the provided
**  arguments.  Returns 0 on success, -1 on failure and prints an error
**  message.
**
**  The service name is imported as a GSSAPI name and a GSS-API context is
**  established with the corresponding service.  The service should be
**  listening on the TCP connection s.  The default GSS-API mechanism is used,
**  and mutual authentication and replay detection are requested.
*/
static int
client_establish_context(char *service_name, gss_ctx_id_t *gss_context,
                         OM_uint32 *ret_flags)
{
    gss_buffer_desc send_tok, recv_tok, *token_ptr;
    gss_name_t target_name;
    OM_uint32 maj_stat, min_stat, init_sec_min_stat;
    int token_flags;

    /* Import the name into target_name.  Use send_tok to save local variable
       space. */
    send_tok.value = service_name;
    send_tok.length = strlen(service_name) + 1;
    maj_stat = gss_import_name(&min_stat, &send_tok, GSS_C_NT_USER_NAME,
                               &target_name);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while parsing name", maj_stat, min_stat);
        return -1;
    }

    if (send_token(TOKEN_NOOP | TOKEN_CONTEXT_NEXT, empty_token) < 0) {
        warn("cannot send initial token to server");
        gss_release_name(&min_stat, &target_name);
        return -1;
    }

    /* Perform the context-establishment loop.

       On each pass through the loop, token_ptr points to the token to send to
       the server (or GSS_C_NO_BUFFER on the first pass).  Every generated
       token is stored in send_tok which is then transmitted to the server;
       every received token is stored in recv_tok, which token_ptr is then set
       to, to be processed by the next call to gss_init_sec_context.

       GSS-API guarantees that send_tok's length will be non-zero if and only
       if the server is expecting another token from us, and that
       gss_init_sec_context returns GSS_S_CONTINUE_NEEDED if and only if the
       server has another token to send us. */
    token_ptr = GSS_C_NO_BUFFER;
    *gss_context = GSS_C_NO_CONTEXT;

    do {
        maj_stat = gss_init_sec_context(&init_sec_min_stat, 
                                        GSS_C_NO_CREDENTIAL, 
                                        gss_context, 
                                        target_name, 
                                        (const gss_OID) GSS_KRB5_MECHANISM,
                                        GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG, 
                                        0, 
                                        NULL,     /* no channel bindings */
                                        token_ptr, 
                                        NULL,     /* ignore mech type */
                                        &send_tok, 
                                        ret_flags, 
                                        NULL);    /* ignore time_rec */

        if (token_ptr != GSS_C_NO_BUFFER)
            gss_release_buffer(&min_stat, &recv_tok);

        if (send_tok.length != 0) {
            debug("sending init_sec_context token, size=%d", send_tok.length);
            if (send_token(TOKEN_CONTEXT, &send_tok) < 0) {
                gss_release_buffer(&min_stat, &send_tok);
                gss_release_name(&min_stat, &target_name);
                return -1;
            }
        }

        gss_release_buffer(&min_stat, &send_tok);

        if (maj_stat != GSS_S_COMPLETE && maj_stat != GSS_S_CONTINUE_NEEDED) {
            display_status("while initializing context", maj_stat,
                           init_sec_min_stat);
            gss_release_name(&min_stat, &target_name);
            if (*gss_context == GSS_C_NO_CONTEXT)
                gss_delete_sec_context(&min_stat, gss_context,
                                       GSS_C_NO_BUFFER);
            return -1;
        }

        if (maj_stat == GSS_S_CONTINUE_NEEDED) {
            debug("continue needed");
            if (recv_token(&token_flags, &recv_tok) < 0) {
                warn("no token received from server");
                gss_release_name(&min_stat, &target_name);
                return -1;
            }
            token_ptr = &recv_tok;
        }
    } while (maj_stat == GSS_S_CONTINUE_NEEDED);

    gss_release_name(&min_stat, &target_name);

    return 0;
}


/*
**  Send the request data to the server.  Takes the context, the argument
**  count, and the argument vector.  Assembles the data payload into the
**  format of [<length><data>]* and then calls gss_sendmsg to send the token
**  over.  Returns 0 on success, -1 on failure.
*/
static int
process_request(gss_ctx_id_t context, OM_uint32 argc, char **argv)
{
    OM_uint32 arglength = 0;
    OM_uint32 network_ordered;
    char *msg;
    int msglength = 0;
    char **cp;           /* Iterator over argv. */
    char *mp;            /* Iterator over msg. */
    OM_uint32 i;

    /* Allocate total message: argc, {<arglength><argument>}+. */
    cp = argv;
    for (i = 0; i < argc; i++) {
        msglength += strlen(*cp++);
    }
    msglength += (argc + 1) * sizeof(OM_uint32);
    msg = xmalloc(msglength);

    /* Iterators over buffers */
    mp = msg;
    cp = argv;
    i = argc;

    /* First, put the argc into the message. */
    network_ordered = htonl(argc);
    memcpy(mp, &network_ordered, sizeof(OM_uint32));
    mp += sizeof(OM_uint32);

    /* Now, pack in the argv.  Arguments are packed: (<arglength><arg>)+. */
    while (i != 0) {
        arglength = strlen(*cp);
        network_ordered = htonl(arglength);
        memcpy(mp, &network_ordered, sizeof(OM_uint32));
        mp += sizeof(OM_uint32);
        memcpy(mp, *cp, arglength);
        mp += arglength;
        i--;
        cp++;
    }

    if (gss_sendmsg(context, TOKEN_DATA, msg, msglength) < 0) {
        free(msg);
        return -1;
    }
    free(msg);
    return 0;
}


/*
**  Get the response back from the server, containing the result message and
**  the exit code.  The response is disassembled and then printed to standard
**  output, and the exit code is returned in the errorcode argument.  Returns
**  0 on success and -1 on failure.
*/
static int
process_response(gss_ctx_id_t context, OM_uint32* errorcode)
{
    char *msg;
    OM_uint32 msglength;
    char *body;      /* Text returned from running the command on the server */
    OM_uint32 bodylength;
    char *cp;        /* Iterator over msg */
    OM_uint32 network_ordered;
    int token_flags;

    if (gss_recvmsg(context, &token_flags, &msg, &msglength) < 0)
        return -1;

    cp = msg;

    /* Extract the return code */
    memcpy(&network_ordered, cp, sizeof(OM_uint32));
    *errorcode = ntohl(network_ordered);
    cp += sizeof(OM_uint32);
    debug("return code: %d", *errorcode);

    /* Get the message length */
    memcpy(&network_ordered, cp, sizeof(OM_uint32));
    cp += sizeof(OM_uint32);
    bodylength = ntohl(network_ordered);
    if (bodylength > MAXBUFFER || bodylength > msglength)
        die("data unpacking error while processing response");

    /* Get the message body */
    body = xmalloc(bodylength + 1);
    memcpy(body, cp, bodylength);
    body[bodylength] = '\0';
    
    printf("%s", body);
    free(msg);
    free(body);

    return (0);
}


/*
**  Main routine.  Parse the arguments, establish the GSSAPI context, and then
**  call process_request and process_response.
*/
int
main(int argc, char *argv[])
{
    int option;
    char *server_host;
    struct hostent *hostinfo;
    char *service_name = NULL;
    unsigned short port = 4444;
    OM_uint32 ret_flags, maj_stat, min_stat;
    int s;
    OM_uint32 errorcode = 0;
    gss_ctx_id_t context;
    gss_buffer_desc out_buf;

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

    if (service_name == NULL) {
        hostinfo = gethostbyname(server_host);
        if (hostinfo == NULL)
            die("cannot resolve host %s", server_host);
        service_name = xmalloc(strlen("host/") + strlen(hostinfo->h_name) + 1);
        strcpy(service_name, "host/");
        strcat(service_name, hostinfo->h_name);
        lowercase(service_name);
    }

    /* Open connection. */
    s = connect_to_server(server_host, port);
    READFD = s;
    WRITEFD = s;

    /* Establish context. */
    if (client_establish_context(service_name, &context, &ret_flags) < 0)
        die("cannot establish GSSAPI context");

    /* Display the flags. */
    display_ctx_flags(ret_flags);

    /* Do the work. */
    if (process_request(context, argc, argv) < 0)
        die("cannot process request");
    if (process_response(context, &errorcode) < 0)
        die("cannot process response");

    /* Delete context and shut down cleanly. */
    maj_stat = gss_delete_sec_context(&min_stat, &context, &out_buf);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while deleting context", maj_stat, min_stat);
        close(s);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        exit(-1);
    }
    gss_release_buffer(&min_stat, &out_buf);
    close(s);

    return errorcode;
}


/*
**  Local variables:
**  mode: c
**  c-basic-offset: 4
**  indent-tabs-mode: nil
**  end:
*/

/*  $Id$
**
**  The client for a "K5 sysctl" - a service for remote execution of 
**  predefined commands.  Access is authenticated via GSSAPI Kerberos 5, 
**  authorized via ACL files.
**
**  Written by Anton Ushakov <antonu@stanford.edu>
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gssapi/gssapi_generic.h>

#include "gss-utils.h"
#include "messages.h"
#include "xmalloc.h"

int verbose;       /* Turns on debugging output. */
int use_syslog;    /* Toggle for sysctl vs stdout/stderr. */

/* These are for storing either the socket for communication with client
   or the streams that talk to the network in case of inetd/tcpserver. */
int READFD;
int WRITEFD;

/* Used in establishing context with gss server. */
gss_buffer_desc empty_token_buf = { 0, (void *) "" };
gss_buffer_t empty_token = &empty_token_buf;


/*
**  Display the usage message for remctl.
*/
static void
usage(void)
{
    static const char usage[] = "\
Usage: remctl <options> host type service <parameters>\n\
Options:\n\
\t-s     remctld service principal (default host/machine.stanford.edu)\n\
\t-v     debugging level of output\n";
    fprintf(stderr, usage);
    exit(1);
}


/*
**  Parse an OID string and determine the mechanism type, returning it in the
**  oid argument.  Prints an error message if there are any problems.
*/
static void
parse_oid(const char *mechanism, gss_OID *oid)
{
    char *mechstr = 0, *cp;
    gss_buffer_desc tok;
    OM_uint32 maj_stat, min_stat;

    if (isdigit(mechanism[0])) {
        mechstr = xmalloc(strlen(mechanism) + 5);
        if (!mechstr) {
            fprintf(stderr, "Couldn't allocate mechanism scratch!\n");
            return;
        }
        sprintf(mechstr, "{ %s }", mechanism);
        for (cp = mechstr; *cp; cp++)
            if (*cp == '.')
                *cp = ' ';
        tok.value = mechstr;
    } else
        tok.value = (char *) mechanism;
    tok.length = strlen(tok.value);
    maj_stat = gss_str_to_oid(&min_stat, &tok, oid);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while str_to_oid", maj_stat, min_stat);
        return;
    }
    if (mechstr)
        free(mechstr);
}


/*
**  Opens a TCP connection to the given hostname and port.  The host name is
**  resolved by gethostbyname and the socket is opened and connected.  Returns
**  the file descriptor or -1 on failure, printing an error message.
*/
static int
connect_to_server(char *host, unsigned short port)
{
    struct sockaddr_in saddr;
    struct hostent *hp;
    int s;

    if ((hp = gethostbyname(host)) == NULL) {
        fprintf(stderr, "Unknown host: %s\n", host);
        return -1;
    }

    saddr.sin_family = hp->h_addrtype;
    memcpy((char *)&saddr.sin_addr, hp->h_addr, sizeof(saddr.sin_addr));
    saddr.sin_port = htons(port);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("creating socket");
        return -1;
    }
    if (connect(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("connecting to server");
        (void)close(s);
        return -1;
    }
    return s;
}


/*
**  Given the service name of the remote server, establish a GSSAPI context
**  and return the context handle and any connection flags in the provided
**  arguments.  Returns 0 on success, -1 on failure and prints an error
**  message.
**
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
    gss_OID oid = GSS_C_NULL_OID;
    gss_buffer_desc send_tok, recv_tok, *token_ptr;
    gss_name_t target_name;
    OM_uint32 maj_stat, min_stat, init_sec_min_stat;
    int token_flags;

    parse_oid("1.2.840.113554.1.2.2", &oid);

    /* Import the name into target_name.  Use send_tok to save local variable
       space. */
    send_tok.value = service_name;
    send_tok.length = strlen(service_name) + 1;
    maj_stat = gss_import_name(&min_stat, &send_tok,
                               (gss_OID) gss_nt_user_name, &target_name);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while parsing name", maj_stat, min_stat);
        return -1;
    }

    if (send_token(TOKEN_NOOP | TOKEN_CONTEXT_NEXT, empty_token) < 0) {
        fprintf(stderr, "Can't send initial token to server\n");
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
                                        oid, 
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
            if (verbose)
                printf("Sending init_sec_context token (size=%d)...\n",
                       send_tok.length);
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
            if (verbose)
                printf("continue needed...");
            if (recv_token(&token_flags, &recv_tok) < 0) {
                fprintf(stderr, "No token received from server\n");
                gss_release_name(&min_stat, &target_name);
                return -1;
            }
            token_ptr = &recv_tok;
        }
        if (verbose)
            printf("\n");
    } while (maj_stat == GSS_S_CONTINUE_NEEDED);

    gss_release_name(&min_stat, &target_name);
    gss_release_oid(&min_stat, &oid);

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

    if (gss_sendmsg(context, TOKEN_DATA, msg, msglength) < 0)
        return (-1);

    return (0);
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
        return (-1);

    cp = msg;

    /* Extract the return code */
    memcpy(&network_ordered, cp, sizeof(OM_uint32));
    *errorcode = ntohl(network_ordered);
    cp += sizeof(OM_uint32);
    if (verbose)
        printf("Return code: %d\n", *errorcode);

    /* Get the message length */
    memcpy(&network_ordered, cp, sizeof(OM_uint32));
    cp += sizeof(OM_uint32);
    bodylength = ntohl(network_ordered);
    if (bodylength > MAXBUFFER || bodylength > msglength) {
        fprintf(stderr, "Data unpacking error while processing response\n");
        exit(-1);
    }

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
main(int argc, char ** argv)
{
    char service_name[500];
    char *server_host;
    struct hostent* hostinfo;
    unsigned short port = 4444;
    OM_uint32 ret_flags, maj_stat, min_stat;
    int s;
    OM_uint32 errorcode = 0;

    gss_ctx_id_t context;
    gss_buffer_desc out_buf;

    service_name[0] = '\0';
    verbose = 0;
    use_syslog = 0;

    /* Set up logging and identity. */
    message_program_name = "remctl";

    /* Parse arguments. */
    argc--;
    argv++;
    while (argc) {
        if (strcmp(*argv, "-p") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            port = atoi(*argv);
        } else if (strcmp(*argv, "-s") == 0) {
            argc--;
            argv++;
            if (!argc)
                usage();
            strncpy(service_name, *argv, sizeof(service_name));
        } else if (strcmp(*argv, "-v") == 0) {
            message_handlers_debug(1, message_log_stdout);
        } else
            break;
        argc--;
        argv++;
    }
    if (argc < 3)
        usage();

    server_host = *argv++;
    argc--;

    if (strlen(service_name) == 0) {
        if ((hostinfo = gethostbyname(server_host)) == NULL) {
            fprintf(stderr, "Can't resolve given hostname\n");
            exit(1);
        }

        lowercase(hostinfo->h_name);
        strcpy(service_name, "host/");
        strcat(service_name, hostinfo->h_name);
    }

    /* Open connection. */
    if ((s = connect_to_server(server_host, port)) < 0)
        return -1;

    READFD = s;
    WRITEFD = s;

    /* Establish context. */
    if (client_establish_context(service_name, &context, &ret_flags) < 0) {
        fprintf(stderr, "Can't establish GSS-API context\n");
        close(s);
        return -1;
    }

    /* Display the flags. */
    if (verbose)
        display_ctx_flags(ret_flags);

    if (process_request(context, argc, argv) < 0) {
        fprintf(stderr, "Can't process request\n");
        exit(1);
    }

    if (process_response(context, &errorcode) < 0) {
        fprintf(stderr, "Can't process response\n");
        exit(1);
    }

    /* Delete context. */
    maj_stat = gss_delete_sec_context(&min_stat, &context, &out_buf);
    if (maj_stat != GSS_S_COMPLETE) {
        display_status("while deleting context", maj_stat, min_stat);
        close(s);
        gss_delete_sec_context(&min_stat, &context, GSS_C_NO_BUFFER);
        return -1;
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

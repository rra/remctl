/*  $Id$
**
**  Protocol v1, client implementation.
**
**  This is the client implementation of the old v1 protocol, which doesn't
**  support streaming, keep-alive, or many of the other features of the
**  current protocol.  We shoehorn this protocol into the same API as the new
**  protocol so that clients don't have to care, but some functions (like
**  continued commands) will return unimplemented errors.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Based on work by Anton Ushakov
**  Copyright 2002, 2003, 2004, 2005, 2006
**      Board of Trustees, Leland Stanford Jr. University
**
**  See README for licensing terms.
*/

#include <config.h>
#include <system.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <util/util.h>

/* We're unwilling to accept tokens from the remote side larger than this.
   FIXME: Figure out what the actual limit is by asking GSSAPI. */
#define MAX_TOKEN       (1024 * 1024)

/*
**  Open a new connection to a server.  Returns true on success, false on
**  failure.  On failure, sets the error message appropriately.
**
**  FIXME: This function assumes protocol version one from the start and never
**  attempts to negotiate protocol version two, which isn't how remctl will
**  work.  It's therefore likely to be rolled into a general connect function
**  that tries either protocol later on.
*/
int
_remctl_v1_open(struct remctl *r, const char *host, unsigned short port,
                const char *principal)
{
    struct sockaddr_in saddr;
    struct hostent *hp;
    int fd, status, flags;
    gss_buffer_desc send_tok, recv_tok, name_buffer, *token_ptr;
    static const gss_buffer_desc empty_token = { 0, (void *) "" };
    gss_name_t name;
    gss_ctx_id_t gss_context;
    OM_uint32 major, minor, init_minor, gss_flags;

    /* Look up the remote host and open a TCP connection. */
    hp = gethostbyname(host);
    if (hp == NULL) {
        _remctl_set_error(r, "unknown host: %s", host);
        return 0;
    }
    saddr.sin_family = hp->h_addrtype;
    memcpy((char *)&saddr.sin_addr, hp->h_addr, sizeof(saddr.sin_addr));
    saddr.sin_port = htons(port);
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        _remctl_set_error(r, "error creating socket: %s", strerror(errno));
        return 0;
    }
    if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        _remctl_set_error(r, "cannot connect to %s (port %hu): %s", host,
                          port, strerror(errno));
        return 0;
    }
    r->fd = fd;

    /* Import the name into target_name. */
    name_buffer.value = principal;
    name_buffer.length = strlen(principal) + 1;
    major = gss_import_name(&minor, &name_buffer, GSS_C_NT_USER_NAME, &name);
    if (major != GSS_S_COMPLETE) {
        _remctl_gssapi_error(r, "parsing name", major, minor);
        return 0;
    }

    /* Send the initial negotiation token. */
    status = token_send(fd, TOKEN_NOOP | TOKEN_CONTEXT_NEXT, &empty_token);
    if (status != TOKEN_OK) {
        _remctl_token_error(r, "sending initial token", status, major, minor);
        gss_release_name(&minor, &name);
        return 0;
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
        major = gss_init_sec_context(&init_minor, GSS_C_NO_CREDENTIAL, 
                    gss_context, name, (const gss_OID) GSS_KRB5_MECHANISM,
                    GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG, 0, NULL,
                    token_ptr, NULL, &send_tok, &gss_flags, NULL);

        if (token_ptr != GSS_C_NO_BUFFER)
            gss_release_buffer(&minor, &recv_tok);

        /* If we have anything more to say, send it. */
        if (send_tok.length != 0) {
            status = token_send(fd, TOKEN_CONTEXT, &send_tok);
            if (status != TOKEN_OK) {
                _remctl_token_error(r, "sending token", status, major, minor);
                gss_release_buffer(&min_stat, &send_tok);
                gss_release_name(&min_stat, &target_name);
                return 0;
            }
        }
        gss_release_buffer(&min_stat, &send_tok);

        /* On error, report the error and abort. */
        if (major != GSS_S_COMPLETE && major != GSS_S_CONTINUE_NEEDED) {
            _remctl_gssapi_error(r, "initializing context", major,
                                 init_minor);
            gss_release_name(&min_stat, &target_name);
            if (*gss_context == GSS_C_NO_CONTEXT)
                gss_delete_sec_context(&minor, gss_context, GSS_C_NO_BUFFER);
            return 0;
        }

        /* If we're still expecting more, retrieve it. */
        if (maj_stat == GSS_S_CONTINUE_NEEDED) {
            status = token_recv(fd, &flags, &recv_tok, MAX_TOKEN);
            if (status != TOKEN_OK) {
                _remctl_token_error(r, "receiving token", status, major,
                                    minor);
                gss_release_name(&minor, &name);
                return 0;
            }
            token_ptr = &recv_tok;
        }
    } while (maj_stat == GSS_S_CONTINUE_NEEDED);

    r->context = gss_context;
    gss_release_name(&minor, &target_name);
    return 1;
}


/*
**  Send a command to the server using protocol v1.  Returns true on success,
**  false on failure.
*/
int
_remctl_v1_commandv(struct remctl *r, const struct iovec *command,
                    size_t count, int finished)
{
    gss_buffer_desc token;
    size_t i;
    char *p;
    OM_uint32 data, major, minor;
    int status;

    /* Partial commands not supported in protocol version one. */
    if (finished != 1) {
        _remctl_set_error(r, "Partial commands not supported");
        return 0;
    }

    /* Allocate room for the total message: argc, {<length><arg>}+. */
    token.length = 4;
    for (i = 0; i < count; i++)
        token.length += 4 + command[i].iov_len;
    token.value = malloc(token.length);
    if (token.value == NULL) {
        _remctl_set_error(r, "Cannot allocate memory: %s", strerror(errno));
        return 0;
    }

    /* First, the argument count.  Then, each argument. */
    p = token.value;
    data = htonl(count);
    memcpy(p, &data, 4);
    p += 4;
    for (i = 0; i < count; i++) {
        data = htonl(command[i].iov_len);
        memcpy(p, &data, 4);
        p += 4;
        memcpy(p, command[i].iov_data, command[i].iov_len);
        p += command[i].iov_len;
    }

    /* Send the result. */
    status = token_send_priv(r->fd, r->context, TOKEN_DATA, &token, &major,
                             &minor);
    if (status != TOKEN_OK) {
        _remctl_token_error(r, "sending token", status, major, minor);
        free(token.value);
        return 0;
    }
    free(token.value);
    return 1;
}


/*
**  Retrieve the output from the server using protocol version one and return
**  it.  This function will actually be called twice, once to retrieve the
**  output data and once to retrieve the exit status.  The old protocol
**  returned those together in one message, so we have to buffer the exit
**  status and return it on the second call.  Returns a remctl output struct
**  on success and NULL on failure.
*/
struct remctl_output *
_remctl_v1_output(struct remctl *r)
{
    int status, flags;
    gss_buffer_desc token;
    OM_uint32 data, major, minor, length;
    char *p;

    /* First, see if we already had an output struct.  If so, this is the
       second call and we should just return the exit status. */
    if (r->output != NULL) {
        if (r->output->type == REMCTL_OUT_STATUS)
            r->output->type = REMCTL_OUT_DONE;
        else {
            _remctl_output_wipe(r->output);
            r->output->type = REMCTL_OUT_STATUS;
        }
        r->output->status = r->status;
        return r->output;
    }

    /* Otherwise, we have to read the token from the server. */
    status = token_recv_priv(r->fd, r->context, &flags, &token, MAX_TOKEN,
                             &major, &minor);
    if (status != TOKEN_OK) {
        _remctl_token_error(r, "receiving token", status, major, minor);
        return NULL;
    }
    if (flags != TOKEN_DATA) {
        _remctl_set_error(r, "Unexpected token from server");
        gss_release_buffer(&minor, &token);
        return NULL;
    }

    /* Extract the return code, message length, and data. */
    if (token.length < 8) {
        _remctl_set_error(r, "Malformed result token from server");
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    p = token.value;
    memcpy(&data, p, 4);
    r->status = ntohl(data);
    p += 4;
    memcpy(&data, p, 4);
    length = ntohl(data);
    p += 4;
    if (length != token.length - 8) {
        _remctl_set_error(r, "Malformed result token from server");
        gss_release_buffer(&minor, &token);
        return NULL;
    }

    /* Allocate the new output token.  We make another copy of the data,
       unfortunately, so that we don't have to keep the token around to free
       later. */
    r->output = malloc(sizeof(struct remctl_output));
    if (r->output != NULL) {
        _remctl_set_error(r, "Cannot allocate memory: %s", strerror(errno));
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    r->output->type = REMCTL_OUT_OUTPUT;
    r->output->data = malloc(length);
    if (r->output->data != NULL) {
        _remctl_set_error(r, "Cannot allocate memory: %s", strerror(errno));
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    memcpy(r->output->data, p, length);
    r->output->length = length;

    /* We always claim everything was stdout since we have no way of knowing
       better with protocol version one. */
    r->output->stream = 1;
    return r->output;
}

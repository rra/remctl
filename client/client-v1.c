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

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <client/internal.h>
#include <client/remctl.h>
#include <util/util.h>

/* We're unwilling to accept tokens from the remote side larger than this.
   FIXME: Figure out what the actual limit is by asking GSSAPI. */
#define MAX_TOKEN       (1024 * 1024)


/*
**  Send a command to the server using protocol v1.  Returns true on success,
**  false on failure.
*/
int
_remctl_v1_commandv(struct remctl *r, const struct iovec *command,
                    size_t count, int finished)
{
    gss_buffer_desc token, mic;
    size_t i;
    char *p;
    OM_uint32 data, major, minor;
    int status, flags;
    gss_qop_t qop_state;

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
        memcpy(p, command[i].iov_base, command[i].iov_len);
        p += command[i].iov_len;
    }

    /* Send the result. */
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_SEND_MIC,
                             &token, &major, &minor);
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
    if (r->output == NULL) {
        _remctl_set_error(r, "Cannot allocate memory: %s", strerror(errno));
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    r->output->type = REMCTL_OUT_OUTPUT;
    r->output->data = malloc(length);
    if (r->output->data == NULL) {
        _remctl_set_error(r, "Cannot allocate memory: %s", strerror(errno));
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    memcpy(r->output->data, p, length);
    r->output->length = length;

    /* We always claim everything was stdout since we have no way of knowing
       better with protocol version one. */
    r->output->stream = 1;

    /* We can only do one round with protocol version one, so close the
       connection now. */
    close(r->fd);
    r->fd = -1;
    return r->output;
}

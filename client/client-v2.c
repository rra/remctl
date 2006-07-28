/*  $Id$
**
**  Protocol v2, client implementation.
**
**  This is the client implementation of the new v2 protocol.  It's fairly
**  close to the regular remctl API.
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
#include <netinet/in.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <client/internal.h>
#include <client/remctl.h>
#include <util/util.h>

/* We're unwilling to accept tokens from the remote side larger than this.
   FIXME: Figure out what the actual limit is by asking GSS-API. */
#define MAX_TOKEN       (1024 * 1024)


/*
**  Send a command to the server using protocol v2.  Returns true on success,
**  false on failure.
*/
int
_remctl_v2_commandv(struct remctl *r, const struct iovec *command,
                    size_t count, int finished)
{
    gss_buffer_desc token;
    size_t i;
    char *p;
    OM_uint32 data, major, minor;
    int status;

    /* Allocate room for the total message. */
    token.length = 1 + 1 + 1 + 1 + 4;
    for (i = 0; i < count; i++)
        token.length += 4 + command[i].iov_len;
    token.value = malloc(token.length);
    if (token.value == NULL) {
        _remctl_set_error(r, "Cannot allocate memory: %s", strerror(errno));
        return 0;
    }

    /* Protocol version. */
    p = token.value;
    *p = 2;
    p++;

    /* Message type. */
    *p = MESSAGE_COMMAND;
    p++;

    /* Keep-alive flag.  Always set to true for now. */
    *p = 1;
    p++;

    /* Continue status. */
    *p = (finished) ? 0 : 1;
    p++;

    /* Argument count and then each argument. */
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
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, &major, &minor);
    if (status != TOKEN_OK) {
        _remctl_token_error(r, "sending token", status, major, minor);
        free(token.value);
        return 0;
    }
    free(token.value);
    r->ready = 1;
    return 1;
}


/*
**  Send a quit command to the server using protocol v2.  Returns true on
**  success, false on failure.
*/
int
_remctl_v2_quit(struct remctl *r)
{
    gss_buffer_desc token;
    char buffer[2] = { 2, MESSAGE_QUIT };
    OM_uint32 major, minor;
    int status;

    token.length = 1 + 1;
    token.value = buffer;
    status = token_send_priv(r->fd, r->context, TOKEN_DATA | TOKEN_PROTOCOL,
                             &token, &major, &minor);
    if (status != TOKEN_OK) {
        _remctl_token_error(r, "sending token", status, major, minor);
        return 0;
    }
    return 1;
}


/*
**  Retrieve the output from the server using protocol v2 and return it.  This
**  function may be called any number of times; if the last packet we got from
**  the server was a REMCTL_OUT_STATUS or REMCTL_OUT_ERROR, we'll return
**  REMCTL_OUT_DONE from that point forward.  Returns a remctl output struct
**  on success and NULL on failure.
*/
struct remctl_output *
_remctl_v2_output(struct remctl *r)
{
    int status, flags;
    gss_buffer_desc token;
    size_t size;
    OM_uint32 data, major, minor;
    char *p;
    int type;

    /* Initialize our output.  If we're not ready to read more data from the
       server, return REMCTL_OUT_DONE. */
    if (r->output == NULL) {
        r->output = malloc(sizeof(struct remctl_output));
        if (r->output == NULL) {
            _remctl_set_error(r, "Cannot allocate memory: %s",
                              strerror(errno));
            return NULL;
        }
        r->output->data = NULL;
    }
    _remctl_output_wipe(r->output);
    if (!r->ready)
        return r->output;

    /* Otherwise, we have to read the token from the server. */
    status = token_recv_priv(r->fd, r->context, &flags, &token, MAX_TOKEN,
                             &major, &minor);
    if (status != TOKEN_OK) {
        _remctl_token_error(r, "receiving token", status, major, minor);
        if (status == TOKEN_FAIL_EOF) {
            close(r->fd);
            r->fd = -1;
        }
        return NULL;
    }
    if (flags != (TOKEN_DATA | TOKEN_PROTOCOL)) {
        _remctl_set_error(r, "Unexpected token from server");
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    if (token.length < 2) {
        _remctl_set_error(r, "Malformed result token from server");
        gss_release_buffer(&minor, &token);
        return NULL;
    }

    /* Extract the message protocol and type. */
    p = token.value;
    if (p[0] != 2) {
        _remctl_set_error(r, "Unexpected protocol %d from server", p[0]);
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    type = p[1];
    p += 2;

    /* Now, what we do depends on the message type. */
    switch (type) {
    case MESSAGE_OUTPUT:
        if (token.length < 2 + 6) {
            _remctl_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->type = REMCTL_OUT_OUTPUT;
        if (p[0] != 1 && p[0] != 2) {
            _remctl_set_error(r, "Unexpected stream %d from server", p[0]);
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->stream = p[0];
        p++;
        memcpy(&data, p, 4);
        p += 4;
        size = ntohl(data);
        if (size != token.length - (p - (char *) token.value)) {
            _remctl_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->data = malloc(size);
        if (r->output->data == NULL) {
            _remctl_set_error(r, "Cannot allocate memory: %s",
                              strerror(errno));
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        memcpy(r->output->data, p, size);
        r->output->length = size;
        break;

    case MESSAGE_STATUS:
        if (token.length < 2 + 1) {
            _remctl_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->type = REMCTL_OUT_STATUS;
        r->output->status = p[0];
        r->ready = 0;
        break;

    case MESSAGE_ERROR:
        if (token.length < 2 + 8) {
            _remctl_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->type = REMCTL_OUT_ERROR;
        memcpy(&data, p, 4);
        p += 4;
        r->output->error = ntohl(data);
        memcpy(&data, p, 4);
        p += 4;
        size = ntohl(data);
        if (size != token.length - (p - (char *) token.value)) {
            _remctl_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->data = malloc(size);
        if (r->output->data == NULL) {
            _remctl_set_error(r, "Cannot allocate memory: %s",
                              strerror(errno));
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        memcpy(r->output->data, p, size);
        r->output->length = size;
        r->ready = 0;
        break;

    default:
        _remctl_set_error(r, "Unknown message type %d from server", type);
        gss_release_buffer(&minor, &token);
        return NULL;
    }

    /* We've finished analyzing the packet.  Return the results. */
    gss_release_buffer(&minor, &token);
    return r->output;
}

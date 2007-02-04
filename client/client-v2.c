/*  $Id$
**
**  Protocol v2, client implementation.
**
**  This is the client implementation of the new v2 protocol.  It's fairly
**  close to the regular remctl API.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  Based on work by Anton Ushakov
**  Copyright 2002, 2003, 2004, 2005, 2006, 2007
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


/*
**  Send a command to the server using protocol v2.  Returns true on success,
**  false on failure.
*/
int
internal_v2_commandv(struct remctl *r, const struct iovec *command,
                     size_t count)
{
    size_t length, iov, offset, sent, left, delta;
    gss_buffer_desc token;
    char *p;
    OM_uint32 data, major, minor;
    int status;

    /* Determine the total length of the message. */
    length = 4;
    for (iov = 0; iov < count; iov++)
        length += 4 + command[iov].iov_len;

    /* Now, loop until we've conveyed the entire message. */
    iov = 0;
    offset = 0;
    sent = 0;
    while (sent < length) {
        if (length - sent > TOKEN_MAX_DATA - 4)
            token.length = TOKEN_MAX_DATA;
        else
            token.length = length - sent + 4;
        token.value = malloc(token.length);
        if (token.value == NULL) {
            internal_set_error(r, "Cannot allocate memory: %s",
                               strerror(errno));
            return 0;
        }
        left = token.length - 4;

        /* Each token begins with the protocol version and message type. */
        p = token.value;
        p[0] = 2;
        p[1] = MESSAGE_COMMAND;
        p += 2;

        /* Keep-alive flag.  Always set to true for now. */
        *p = 1;
        p++;

        /* Continue status. */
        if (token.length == length - sent + 4)
            *p = (sent == 0) ? 0 : 3;
        else
            *p = (sent == 0) ? 1 : 2;
        p++;

        /* Argument count if we haven't sent anything yet. */
        if (sent == 0) {
            data = htonl(count);
            memcpy(p, &data, 4);
            p += 4;
            sent += 4;
            left -= 4;
        }

        /* Now, as many arguments as will fit. */
        for (; iov < count; iov++) {
            if (offset == 0) {
                if (left < 4)
                    break;
                data = htonl(command[iov].iov_len);
                memcpy(p, &data, 4);
                p += 4;
                sent += 4;
                left -= 4;
            }
            if (left >= command[iov].iov_len - offset)
                delta = command[iov].iov_len - offset;
            else
                delta = left;
            memcpy(p, (char *) command[iov].iov_base + offset, delta);
            p += delta;
            sent += delta;
            offset += delta;
            left -= delta;
            if (offset < command[iov].iov_len)
                break;
            offset = 0;
        }

        /* Send the result. */
        token.length -= left;
        status = token_send_priv(r->fd, r->context,
                                 TOKEN_DATA | TOKEN_PROTOCOL, &token,
                                 &major, &minor);
        if (status != TOKEN_OK) {
            internal_token_error(r, "sending token", status, major, minor);
            free(token.value);
            return 0;
        }
        free(token.value);
    }
    r->ready = 1;
    return 1;
}


/*
**  Send a quit command to the server using protocol v2.  Returns true on
**  success, false on failure.
*/
int
internal_v2_quit(struct remctl *r)
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
        internal_token_error(r, "sending token", status, major, minor);
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
internal_v2_output(struct remctl *r)
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
            internal_set_error(r, "Cannot allocate memory: %s",
                               strerror(errno));
            return NULL;
        }
        r->output->data = NULL;
    }
    internal_output_wipe(r->output);
    if (!r->ready)
        return r->output;

    /* Otherwise, we have to read the token from the server. */
    status = token_recv_priv(r->fd, r->context, &flags, &token,
                             TOKEN_MAX_LENGTH, &major, &minor);
    if (status != TOKEN_OK) {
        internal_token_error(r, "receiving token", status, major, minor);
        if (status == TOKEN_FAIL_EOF) {
            close(r->fd);
            r->fd = -1;
        }
        return NULL;
    }
    if (flags != (TOKEN_DATA | TOKEN_PROTOCOL)) {
        internal_set_error(r, "Unexpected token from server");
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    if (token.length < 2) {
        internal_set_error(r, "Malformed result token from server");
        gss_release_buffer(&minor, &token);
        return NULL;
    }

    /* Extract the message protocol and type. */
    p = token.value;
    if (p[0] != 2) {
        internal_set_error(r, "Unexpected protocol %d from server", p[0]);
        gss_release_buffer(&minor, &token);
        return NULL;
    }
    type = p[1];
    p += 2;

    /* Now, what we do depends on the message type. */
    switch (type) {
    case MESSAGE_OUTPUT:
        if (token.length < 2 + 6) {
            internal_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->type = REMCTL_OUT_OUTPUT;
        if (p[0] != 1 && p[0] != 2) {
            internal_set_error(r, "Unexpected stream %d from server", p[0]);
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->stream = p[0];
        p++;
        memcpy(&data, p, 4);
        p += 4;
        size = ntohl(data);
        if (size != token.length - (p - (char *) token.value)) {
            internal_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->data = malloc(size);
        if (r->output->data == NULL) {
            internal_set_error(r, "Cannot allocate memory: %s",
                               strerror(errno));
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        memcpy(r->output->data, p, size);
        r->output->length = size;
        break;

    case MESSAGE_STATUS:
        if (token.length < 2 + 1) {
            internal_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->type = REMCTL_OUT_STATUS;
        r->output->status = p[0];
        r->ready = 0;
        break;

    case MESSAGE_ERROR:
        if (token.length < 2 + 8) {
            internal_set_error(r, "Malformed result token from server");
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
            internal_set_error(r, "Malformed result token from server");
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        r->output->data = malloc(size);
        if (r->output->data == NULL) {
            internal_set_error(r, "Cannot allocate memory: %s",
                              strerror(errno));
            gss_release_buffer(&minor, &token);
            return NULL;
        }
        memcpy(r->output->data, p, size);
        r->output->length = size;
        r->ready = 0;
        break;

    default:
        internal_set_error(r, "Unknown message type %d from server", type);
        gss_release_buffer(&minor, &token);
        return NULL;
    }

    /* We've finished analyzing the packet.  Return the results. */
    gss_release_buffer(&minor, &token);
    return r->output;
}

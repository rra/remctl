/*
 * Protocol v1, server implementation.
 *
 * This is the server implementation of the old v1 protocol, which doesn't
 * support streaming, keep-alive, or many of the other features of the
 * current protocol.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2016 Dropbox, Inc.
 * Copyright 2002-2010, 2012, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/event.h>
#include <portable/gssapi.h>
#include <portable/socket.h>
#include <portable/system.h>
#include <portable/uio.h>

#include <server/internal.h>
#include <util/gss-tokens.h>
#include <util/macros.h>
#include <util/messages.h>
#include <util/xmalloc.h>


/*
 * Discard all data in the evbuffer.  This handler is used with protocol
 * version one when we've already read as much data as we can return to the
 * remctl client.
 */
static void
handle_output_discard(struct bufferevent *bev, void *data UNUSED)
{
    size_t length;
    struct evbuffer *buf;

    buf = bufferevent_get_input(bev);
    length = evbuffer_get_length(buf);
    if (evbuffer_drain(buf, length) < 0)
        sysdie("internal error: cannot discard extra output");
}


/*
 * Callback used to handle filling the output buffer with protocol version
 * one.  When this happens, we pull all of the data out into a separate
 * evbuffer and then change our read callback to handle_output_discard, which
 * just drains (discards) all subsequent data from the process.
 */
static void
handle_output_full(struct bufferevent *bev, void *data)
{
    struct process *process = data;
    bufferevent_data_cb writecb;

    process->output = evbuffer_new();
    if (process->output == NULL)
        die("internal error: cannot create discard evbuffer");
    if (bufferevent_read_buffer(bev, process->output) < 0)
        die("internal error: cannot move data into output buffer");

    /*
     * Change the output callback.  We need to be sure not to dump our input
     * callback if it exists.
     *
     * After we see all the output that we can send to the client, we no
     * longer care about error and EOF events, but if we set the callback to
     * NULL here, we cause segfaults in libevent 1.4.x when we have both read
     * and EOF events in the same event loop.  So keep the error event handler
     * since it doesn't hurt anything.  This can safely be set to NULL once we
     * require libevent 2.x.
     */
    writecb = (process->input == NULL) ? NULL : server_handle_input_end;
    bufferevent_setcb(bev, handle_output_discard, writecb,
                      server_handle_io_event, data);
}


/*
 * Set up handling of a child process with the v1 protocol.  Takes the process
 * struct sets up the necessary event loop hooks.
 */
void
server_v1_command_setup(struct process *process)
{
    bufferevent_data_cb writecb;

    writecb = (process->input == NULL) ? NULL : server_handle_input_end;
    bufferevent_setcb(process->inout, handle_output_full, writecb,
                      server_handle_io_event, process);
    bufferevent_setwatermark(process->inout, EV_READ, TOKEN_MAX_OUTPUT_V1,
                             TOKEN_MAX_OUTPUT_V1);
}


/*
 * Given the client struct, a buffer of data to send, and the exit status of a
 * command, send a protocol v1 output token back to the client.  Returns true
 * on success and false on failure (and logs a message on failure).
 */
bool
server_v1_send_output(struct client *client, struct evbuffer *output,
                      int exit_status)
{
    gss_buffer_desc token;
    size_t outlen;
    char *p;
    OM_uint32 tmp, major, minor;
    int status;

    /* Allocate room for the total message. */
    outlen = evbuffer_get_length(output);
    if (outlen >= UINT32_MAX - 4 - 4)
        die("internal error: memory allocation too large");
    token.length = 4 + 4 + outlen;
    token.value = xmalloc(token.length);

    /* Fill in the token. */
    p = token.value;
    tmp = htonl(exit_status);
    memcpy(p, &tmp, 4);
    p += 4;
    tmp = htonl((OM_uint32) outlen);
    memcpy(p, &tmp, 4);
    p += 4;
    if (evbuffer_remove(output, p, outlen) < 0)
        die("internal error: cannot move data from output buffer");
    
    /* Send the token. */
    status = token_send_priv(client->fd, client->context, TOKEN_DATA, &token,
                             TIMEOUT, &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending output token", status, major, minor);
        free(token.value);
        return false;
    }
    free(token.value);
    return true;
}


/*
 * Given the client struct, an error code, and an error message, send a
 * protocol v1 error token to the client.  Returns true on success, false on
 * failure (and logs a message on failure).
 */
bool
server_v1_send_error(struct client *client, enum error_codes code UNUSED,
                     const char *message)
{
    struct evbuffer *buf;
    bool result;

    buf = evbuffer_new();
    if (buf == NULL)
        die("internal error: cannot create output buffer");
    if (evbuffer_add_printf(buf, "%s\n", message) < 0)
        die("internal error: cannot add error message to buffer");
    result = server_v1_send_output(client, buf, -1);
    evbuffer_free(buf);
    return result;
}


/*
 * Takes the client struct and the server configuration and handles a client
 * request.  Reads a command from the client, checks the ACL, runs the command
 * if appropriate, and sends any output back to the client.
*/
void
server_v1_handle_messages(struct client *client, struct config *config)
{
    gss_buffer_desc token;
    OM_uint32 major, minor;
    struct iovec **argv = NULL;
    int status, flags;

    /* Receive the message. */
    status = token_recv_priv(client->fd, client->context, &flags, &token,
                             TOKEN_MAX_LENGTH, TIMEOUT, &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("receiving command token", status, major, minor);
        if (status == TOKEN_FAIL_LARGE)
            client->error(client, ERROR_TOOMUCH_DATA, "Too much data");
        else if (status != TOKEN_FAIL_EOF)
            client->error(client, ERROR_BAD_TOKEN, "Invalid token");
        return;
    }

    /* Check the data size. */
    if (token.length > TOKEN_MAX_DATA) {
        warn("command data length %lu exceeds 64KB",
             (unsigned long) token.length);
        client->error(client, ERROR_TOOMUCH_DATA, "Too much data");
        gss_release_buffer(&minor, &token);
        return;
    }

    /*
     * Do the shared parsing of the message.  This code is identical to the
     * code for v2 (v2 just pulls more data off the front of the token first).
     */
    argv = server_parse_command(client, token.value, token.length);
    gss_release_buffer(&minor, &token);
    if (argv == NULL)
        return;

    /*
     * Check the ACL and existence of the command, run the command if
     * possible, and accumulate the output in the client struct.
     */
    server_run_command(client, config, argv);
    server_free_command(argv);
}

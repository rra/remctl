/*
 * Protocol v2, server implementation.
 *
 * This is the server implementation of the new v2 protocol.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Based on work by Anton Ushakov
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/system.h>
#include <portable/gssapi.h>
#include <portable/socket.h>
#include <portable/uio.h>

#include <server/internal.h>
#include <util/gss-tokens.h>
#include <util/messages.h>
#include <util/xmalloc.h>


/*
 * Given the client struct and the stream number the data is from, send a
 * protocol v2 output token to the client containing the data stored in the
 * buffer in the client struct.  Returns true on success, false on failure
 * (and logs a message on failure).
 */
bool
server_v2_send_output(struct client *client, int stream)
{
    gss_buffer_desc token;
    char *p;
    OM_uint32 tmp, major, minor;
    int status;

    /* Allocate room for the total message. */
    token.length = 1 + 1 + 1 + 4 + client->outlen;
    token.value = xmalloc(token.length);

    /*
     * Fill in the header (version, type, stream, length, and data) and then
     * the data.
     */
    p = token.value;
    *p = 2;
    p++;
    *p = MESSAGE_OUTPUT;
    p++;
    *p = stream;
    p++;
    tmp = htonl(client->outlen);
    memcpy(p, &tmp, 4);
    p += 4;
    memcpy(p, client->output, client->outlen);

    /* Send the token. */
    status = token_send_priv(client->fd, client->context,
                             TOKEN_DATA | TOKEN_PROTOCOL, &token, TIMEOUT,
                             &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending output token", status, major, minor);
        free(token.value);
        client->fatal = true;
        return false;
    }
    free(token.value);
    return true;
}


/*
 * Given the client struct and the exit status, send a protocol v2 status
 * token to the client.  Returns true on success, false on failure (and logs a
 * message on failure).
 */
bool
server_v2_send_status(struct client *client, int exit_status)
{
    gss_buffer_desc token;
    char buffer[1 + 1 + 1];
    OM_uint32 major, minor;
    int status;

    /* Build the status token. */
    token.length = 1 + 1 + 1;
    token.value = &buffer;
    buffer[0] = 2;
    buffer[1] = MESSAGE_STATUS;
    buffer[2] = exit_status;

    /* Send the token. */
    status = token_send_priv(client->fd, client->context,
                             TOKEN_DATA | TOKEN_PROTOCOL, &token, TIMEOUT,
                             &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending status token", status, major, minor);
        client->fatal = true;
        return false;
    }
    return true;
}


/*
 * Given the client struct, an error code, and an error message, send a
 * protocol v2 error token to the client.  Returns true on success, false on
 * failure (and logs a message on failure).
 */
bool
server_v2_send_error(struct client *client, enum error_codes code,
                     const char *message)
{
    gss_buffer_desc token;
    char *p;
    OM_uint32 tmp, major, minor;
    int status;

    /* Build the error token. */
    token.length = 1 + 1 + 4 + 4 + strlen(message);
    token.value = xmalloc(token.length);
    p = token.value;
    *p = 2;
    p++;
    *p = MESSAGE_ERROR;
    p++;
    tmp = htonl(code);
    memcpy(p, &tmp, 4);
    p += 4;
    tmp = htonl(strlen(message));
    memcpy(p, &tmp, 4);
    p += 4;
    memcpy(p, message, strlen(message));

    /* Send the token. */
    status = token_send_priv(client->fd, client->context,
                             TOKEN_DATA | TOKEN_PROTOCOL, &token, TIMEOUT,
                             &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending error token", status, major, minor);
        free(token.value);
        client->fatal = true;
        return false;
    }
    free(token.value);
    return true;
}


/*
 * Given the client struct, send a protocol v2 version token to the client.
 * This is the response to a higher version number than we understand.
 * Returns true on success, false on failure (and logs a message on failure).
 */
static bool
server_v2_send_version(struct client *client)
{
    gss_buffer_desc token;
    char buffer[1 + 1 + 1];
    OM_uint32 major, minor;
    int status;

    /* Build the version token. */
    token.length = 1 + 1 + 1;
    token.value = &buffer;
    buffer[0] = 2;
    buffer[1] = MESSAGE_VERSION;
    buffer[2] = 3;

    /* Send the token. */
    status = token_send_priv(client->fd, client->context,
                             TOKEN_DATA | TOKEN_PROTOCOL, &token, TIMEOUT,
                             &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending version token", status, major, minor);
        client->fatal = true;
        return false;
    }
    return true;
}


/*
 * Given the client struct, send a protocol v3 no-op token to the client.
 * This is the response to a no-op token.  Returns true on success, false on
 * failure (and logs a message on failure).
 */
static bool
server_v3_send_noop(struct client *client)
{
    gss_buffer_desc token;
    char buffer[1 + 1];
    OM_uint32 major, minor;
    int status;

    /* Build the version token. */
    token.length = 1 + 1;
    token.value = &buffer;
    buffer[0] = 3;
    buffer[1] = MESSAGE_NOOP;

    /* Send the token. */
    status = token_send_priv(client->fd, client->context,
                             TOKEN_DATA | TOKEN_PROTOCOL, &token, TIMEOUT,
                             &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending no-op token", status, major, minor);
        client->fatal = true;
        return false;
    }
    return true;
}


/*
 * Receive a new token from the client, handling reporting of errors.  Takes
 * the client struct and a pointer to storage for the token.  Returns TOKEN_OK
 * on success, TOKEN_FAIL_EOF if the other end has gone away, and a different
 * error code on a recoverable error.
 */
static int
server_v2_read_token(struct client *client, gss_buffer_t token)
{
    OM_uint32 major, minor;
    int status, flags;
    
    status = token_recv_priv(client->fd, client->context, &flags, token,
                             TOKEN_MAX_LENGTH, TIMEOUT, &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("receiving token", status, major, minor);
        if (status != TOKEN_FAIL_EOF && status != TOKEN_FAIL_SOCKET)
            server_send_error(client, ERROR_BAD_TOKEN, "Invalid token");
    }
    return status;
}


/*
 * Read a continuation token for a command.  This handles checking the message
 * version, verifying that it's a command token, handling MESSAGE_QUIT, and so
 * forth.  It's almost but not quite the same as the processing in
 * server_v2_handle_token.  Stores the token in the provided token argument
 * and returns true if a valid token was received.  Returns false if an
 * invalid token was received or if some other error occurred, or if
 * MESSAGE_QUIT was received.  False should result in aborting the pending
 * command.
 */
static bool
server_v2_read_continuation(struct client *client, gss_buffer_t token)
{
    int status;
    char *p;

    status = server_v2_read_token(client, token);
    if (status != TOKEN_OK) {
        client->fatal = true;
        return false;
    }
    p = token->value;
    if (p[0] != 2 && p[0] != 3) {
        server_v2_send_version(client);
        return false;
    } else if (p[1] == MESSAGE_QUIT) {
        debug("quit received, aborting command and closing connection");
        client->keepalive = false;
        return false;
    } else if (p[1] != MESSAGE_COMMAND) {
        warn("unexpected message type %d from client", (int) p[1]);
        server_send_error(client, ERROR_UNEXPECTED_MESSAGE,
                          "Unexpected message");
        return false;
    }
    return true;
}


/*
 * Handles a single command message from the client, responding or running the
 * command as appropriate.  Returns true if we should continue to process
 * further messages on that connection, and false if a fatal error occurred
 * and the connection should be closed.
 */
static bool
server_v2_handle_command(struct client *client, struct config *config,
                         gss_buffer_t token)
{
    char *p;
    size_t length, total;
    char *buffer = NULL;
    OM_uint32 minor;
    struct iovec **argv = NULL;
    bool result = false;
    bool allocated = false;
    bool continued = false;

    /*
     * Loop on tokens until we have a complete command, allowing for continued
     * commands.  We're going to accumulate the full command in buffer until
     * we've seen all of it.  If the command isn't continued, we can use the
     * token as the buffer.
     */
    total = 0;
    do {
        p = token->value;
        client->keepalive = p[2] ? true : false;

        /* Check the data size. */
        if (token->length > TOKEN_MAX_DATA) {
            warn("command data length %lu exceeds 64KB",
                 (unsigned long) token->length);
            result = server_send_error(client, ERROR_TOOMUCH_DATA,
                                       "Too much data");
            goto fail;
        }

        /* Make sure the continuation is sane. */
        if ((p[3] == 1 && continued) || (p[3] > 1 && !continued) || p[3] > 3) {
            warn("bad continue status %d", (int) p[3]);
            result = server_send_error(client, ERROR_BAD_COMMAND,
                                       "Invalid command token");
            goto fail;
        }
        continued = (p[3] == 1 || p[3] == 2);

        /*
         * Read the token data.  If the command is continued *or* if buffer is
         * non-NULL (meaning the command was previously continued), we copy
         * the data into the buffer.
         */
        p += 4;
        length = token->length - (p - (char *) token->value);
        if (continued || buffer != NULL) {
            if (buffer == NULL)
                buffer = xmalloc(length);
            else
                buffer = xrealloc(buffer, total + length);
            allocated = true;
            memcpy(buffer + total, p, length);
            total += length;
        }

        /*
         * If the command was continued, we have to read the next token.
         * Otherwise, if buffer is NULL (no continuation), we just use this
         * token as the complete buffer.
         */
        if (continued) {
            gss_release_buffer(&minor, token);
            if (!server_v2_read_continuation(client, token))
                goto fail;
        } else if (buffer == NULL) {
            buffer = p;
            total = length;
        }
    } while (continued);

    /*
     * Okay, we now have a complete command that was possibly spread over
     * multiple tokens.  Now we can parse it.
     */
    argv = server_parse_command(client, buffer, total);
    if (allocated)
        free(buffer);
    if (argv == NULL)
        return !client->fatal;

    /* We have a command.  Now do the heavy lifting. */
    server_run_command(client, config, argv);
    server_free_command(argv);
    return !client->fatal;

fail:
    if (allocated)
        free(buffer);
    return client->fatal ? false : result;
}


/*
 * Handles a single token from the client, responding or running a command as
 * appropriate.  Returns true if we should continue processing messages, false
 * if a fatal error occurred (like a network error), a command was sent
 * without keep-alive, or QUIT was received and we should stop processing
 * tokens.
 */
static bool
server_v2_handle_token(struct client *client, struct config *config,
                       gss_buffer_t token)
{
    char *p;
    bool result = true;

    p = token->value;
    if (p[0] != 2 && p[0] != 3)
        return server_v2_send_version(client);
    switch (p[1]) {
    case MESSAGE_COMMAND:
        result = server_v2_handle_command(client, config, token);
        break;
    case MESSAGE_NOOP:
        debug("replying to no-op message");
        result = server_v3_send_noop(client);
        break;
    case MESSAGE_QUIT:
        debug("quit received, closing connection");
        client->keepalive = false;
        result = false;
        break;
    default:
        warn("unknown message type %d from client", (int) p[1]);
        result = server_send_error(client, ERROR_UNKNOWN_MESSAGE,
                                   "Unknown message");
        break;
    }
    return result;
}


/*
 * Takes the client struct and the server configuration and handles client
 * requests.  Reads messages from the client, checking commands against the
 * ACLs and executing them when appropriate, until the connection is
 * terminated.
 */
void
server_v2_handle_messages(struct client *client, struct config *config)
{
    gss_buffer_desc token;
    OM_uint32 minor;
    int status;

    /* Loop receiving messages until we're finished. */
    client->keepalive = true;
    do {
        status = server_v2_read_token(client, &token);
        if (status != TOKEN_OK)
            break;
        if (!server_v2_handle_token(client, config, &token)) {
            gss_release_buffer(&minor, &token);
            break;
        }
        gss_release_buffer(&minor, &token);
    } while (client->keepalive);
}

/*  $Id$
**
**  Protocol v2, server implementation.
**
**  This is the server implementation of the new v2 protocol.
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

#include <netinet/in.h>

#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi_generic.h>
#endif

#include <server/internal.h>
#include <util/util.h>


/*
**  Given the client struct and the stream number the data is from, send a
**  protocol v2 output token to the client containing the data stored in the
**  buffer in the client struct.  Returns true on success, false on failure
**  (and logs a message on failure).
*/
int
server_v2_send_output(struct client *client, int stream)
{
    gss_buffer_desc token;
    char *p;
    OM_uint32 tmp, major, minor;
    int status;

    /* Allocate room for the total message. */
    token.length = 1 + 1 + 1 + 4 + client->outlen;
    token.value = xmalloc(token.length);

    /* Fill in the header (version, type, stream, length, and data) and then
       the data. */
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
                 TOKEN_DATA | TOKEN_PROTOCOL, &token, &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending output token", status, major, minor);
        free(token.value);
        client->fatal = 1;
        return 0;
    }
    free(token.value);
    return 1;
}


/*
**  Given the client struct and the exit status, send a protocol v2 status
**  token to the client.  Returns true on success, false on failure (and logs
**  a message on failure).
*/
int
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
                 TOKEN_DATA | TOKEN_PROTOCOL, &token, &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending status token", status, major, minor);
        client->fatal = 1;
        return 0;
    }
    return 1;
}


/*
**  Given the client struct, an error code, and an error message, send a
**  protocol v2 error token to the client.  Returns true on success, false on
**  failure (and logs a message on failure).
*/
int
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
                 TOKEN_DATA | TOKEN_PROTOCOL, &token, &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending error token", status, major, minor);
        free(token.value);
        client->fatal = 1;
        return 0;
    }
    free(token.value);
    return 1;
}


/*
**  Given the client struct, send a protocol v2 version token to the client.
**  This is the response to a higher version number than we understand.
**  Returns true on success, false on failure (and logs a message on
**  failure).
*/
static int
server_v2_send_version(struct client *client)
{
    gss_buffer_desc token;
    char *p;
    OM_uint32 major, minor;
    int status;

    /* Build the version token. */
    token.length = 1 + 1 + 1;
    token.value = xmalloc(token.length);
    p = token.value;
    *p = 2;
    p++;
    *p = MESSAGE_VERSION;
    p++;
    *p = 2;

    /* Send the token. */
    status = token_send_priv(client->fd, client->context,
                 TOKEN_DATA | TOKEN_PROTOCOL, &token, &major, &minor);
    if (status != TOKEN_OK) {
        warn_token("sending version token", status, major, minor);
        free(token.value);
        client->fatal = 1;
        return 0;
    }
    free(token.value);
    return 1;
}


/*
**  Handles a single token from the client, responding or running a command as
**  appropriate.  Returns true if we should continue, false if an error
**  occurred or QUIT was received and we should stop processing tokens.
*/
static int
server_v2_handle_token(struct client *client, struct config *config,
                       gss_buffer_t token)
{
    char *p;
    size_t length;
    struct vector *argv = NULL;

    /* Parse the token. */
    p = token->value;
    if (p[0] != 2)
        return server_v2_send_version(client);
    if (p[1] == MESSAGE_QUIT) {
        debug("quit received, closing connection");
        return 0;
    } else if (p[1] != MESSAGE_COMMAND) {
        warn("unknown message type %d from client", (int) p[1]);
        return server_send_error(client, ERROR_UNKNOWN_MESSAGE,
                                 "Unknown message");
    }
    p += 2;
    if (p[0])
        client->keepalive = 1;
    if (p[1]) {
        warn("continued command attempted");
        return server_send_error(client, ERROR_BAD_COMMAND,
                                 "Invalid command token");
    }
    p += 2;
    length = token->length - (p - (char *) token->value);
    argv = server_parse_command(client, p, length);
    if (argv == NULL)
        return !client->fatal;

    /* We have a command.  Now do the heavy lifting. */
    server_run_command(client, config, argv);
    return !client->fatal;
}


/*
**  Takes the client struct and the server configuration and handles client
**  requests.  Reads messages from the client, checking commands against the
**  ACLs and executing them when appropriate, until the connection is
**  terminated.
*/
void
server_v2_handle_commands(struct client *client, struct config *config)
{
    gss_buffer_desc token;
    OM_uint32 major, minor;
    int status, flags;

    /* Loop receiving messages until we're finished. */
    while (1) {
        status = token_recv_priv(client->fd, client->context, &flags, &token,
                                 MAX_TOKEN, &major, &minor);
        if (status != TOKEN_OK) {
            warn_token("receiving command token", status, major, minor);
            if (!server_send_error(client, ERROR_BAD_TOKEN, "Invalid token"))
                break;
            continue;
        }
        if (!server_v2_handle_token(client, config, &token))
            break;
    }
}

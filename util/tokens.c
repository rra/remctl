/*
 * Token handling routines.
 *
 * Low-level routines to send and receive remctl tokens.  token_send and
 * token_recv do not do anything to their provided input or output except
 * wrapping flags and a length around them.
 *
 * Originally written by Anton Ushakov
 * Extensive modifications by Russ Allbery <eagle@eyrie.org>
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#include <config.h>
#include <portable/gssapi.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <errno.h>
#include <time.h>

#include <util/messages.h>
#include <util/network.h>
#include <util/tokens.h>
#include <util/xwrite.h>


/*
 * Given a socket errno, map it to one of our error codes.
 */
static enum token_status
map_socket_error(int err)
{
    switch (err) {
    case EPIPE:      return TOKEN_FAIL_EOF;
#ifdef ECONNRESET
    case ECONNRESET: return TOKEN_FAIL_EOF;
#endif
    case ETIMEDOUT:  return TOKEN_FAIL_TIMEOUT;
    default:         return TOKEN_FAIL_SOCKET;
    }
}


/*
 * Send a token to a file descriptor.  Takes the file descriptor, the token,
 * and the flags (a single byte, even though they're passed in as an integer)
 * and writes them to the file descriptor.  Returns TOKEN_OK on success and
 * TOKEN_FAIL_SYSTEM, TOKEN_FAIL_SOCKET, or TOKEN_FAIL_TIMEOUT on an error
 * (including partial writes).
 */
enum token_status
token_send(socket_type fd, int flags, gss_buffer_t tok, time_t timeout)
{
    size_t buflen;
    char *buffer;
    bool okay;
    unsigned char char_flags = (unsigned char) flags;
    OM_uint32 len = htonl(tok->length);

    /* Send out the whole message in a single write. */
    buflen = 1 + sizeof(OM_uint32) + tok->length;
    buffer = malloc(buflen);
    if (buffer == NULL)
        return TOKEN_FAIL_SYSTEM;
    memcpy(buffer, &char_flags, 1);
    memcpy(buffer + 1, &len, sizeof(OM_uint32));
    memcpy(buffer + 1 + sizeof(OM_uint32), tok->value, tok->length);
    okay = network_write(fd, buffer, buflen, timeout);
    free(buffer);
    return okay ? TOKEN_OK : map_socket_error(socket_errno);
}


/*
 * Receive a token from a file descriptor.  Takes the file descriptor, a
 * buffer into which to store the token, a pointer into which to store the
 * flags, and the maximum token length we're willing to accept.  Returns
 * TOKEN_OK on success.  On failure, returns one of:
 *
 *     TOKEN_FAIL_SYSTEM       System call failed, errno set
 *     TOKEN_FAIL_SOCKET       Socket call failed, socket_errno set
 *     TOKEN_FAIL_INVALID      Invalid token format
 *     TOKEN_FAIL_LARGE        Token data larger than provided limit
 *     TOKEN_FAIL_EOF          Unexpected end of file
 *     TOKEN_FAIL_TIMEOUT      Timeout receiving token
 *
 * TOKEN_FAIL_SYSTEM and TOKEN_FAIL_SOCKET are the same on UNIX but different
 * on Windows.
 *
 * recv_token reads the token flags (a single byte, even though they're stored
 * into an integer, then reads the token length (as a network long), allocates
 * memory to hold the data, and then reads the token data from the file
 * descriptor.  On a successful return, the value member of the token should
 * be freed with free().
 */
enum token_status
token_recv(socket_type fd, int *flags, gss_buffer_t tok, size_t max,
           time_t timeout)
{
    OM_uint32 len;
    unsigned char char_flags;
    int err;

    if (!network_read(fd, &char_flags, 1, timeout))
        return map_socket_error(socket_errno);
    *flags = char_flags;

    if (!network_read(fd, &len, sizeof(OM_uint32), timeout))
        return map_socket_error(socket_errno);
    tok->length = ntohl(len);
    if (tok->length > max)
        return TOKEN_FAIL_LARGE;
    if (tok->length == 0) {
        tok->value = NULL;
        return TOKEN_OK;
    }

    tok->value = malloc(tok->length);
    if (tok->value == NULL)
        return TOKEN_FAIL_SYSTEM;
    if (!network_read(fd, tok->value, tok->length, timeout)) {
        err = socket_errno;
        free(tok->value);
        socket_set_errno(err);
        return map_socket_error(err);
    }
    return TOKEN_OK;
}

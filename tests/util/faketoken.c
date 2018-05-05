/*
 * Fake token_send and token_recv functions for testing.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006, 2009-2010, 2012
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/gssapi.h>
#include <portable/socket.h>
#include <portable/system.h>

#include <time.h>

#include <tests/util/faketoken.h>
#include <util/macros.h>
#include <util/tokens.h>

/* The data, length, and flags sent by the last fake_token_send. */
char send_buffer[2048];
size_t send_length;
int send_flags;

/* The data, length, and flags returned by the next fake_token_recv. */
char recv_buffer[2048];
size_t recv_length;
int recv_flags;

/* If set to true, return timeout from the fake token functions. */
bool fail_timeout = false;


/*
 * Accept a token write request and store it into the buffer.
 */
enum token_status
fake_token_send(socket_type fd UNUSED, int flags, gss_buffer_t tok,
                time_t timeout)
{
    if (tok->length > sizeof(send_buffer))
        return TOKEN_FAIL_SYSTEM;
    if (fail_timeout && timeout > 0)
        return TOKEN_FAIL_TIMEOUT;
    send_flags = flags;
    send_length = tok->length;
    memcpy(send_buffer, tok->value, tok->length);
    return TOKEN_OK;
}


/*
 * Receive a token from the stored buffer and return it.
 */
enum token_status
fake_token_recv(socket_type fd UNUSED, int *flags, gss_buffer_t tok,
                size_t max, time_t timeout)
{
    if (recv_length > max)
        return TOKEN_FAIL_LARGE;
    if (fail_timeout && timeout > 0)
        return TOKEN_FAIL_TIMEOUT;
    tok->value = malloc(recv_length);
    if (tok->value == NULL)
        return TOKEN_FAIL_SYSTEM;
    memcpy(tok->value, recv_buffer, recv_length);
    tok->length = recv_length;
    *flags = recv_flags;
    return TOKEN_OK;
}

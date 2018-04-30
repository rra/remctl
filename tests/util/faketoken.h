/*
 * Testing interface to fake token functions.
 *
 * This header defines the interfaces to fake token functions used to test the
 * utility functions for sending and retrieving GSS-API tokens.
 *
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TESTS_UTIL_FAKETOKEN_H
#define TESTS_UTIL_FAKETOKEN_H 1

#include <config.h>
#include <portable/gssapi.h>
#include <portable/macros.h>
#include <portable/socket.h>

#include <sys/types.h>
#include <time.h>

#include <util/tokens.h>

BEGIN_DECLS

/* Replacement functions called instead of normal utility functions. */
enum token_status fake_token_send(socket_type, int, gss_buffer_t, time_t);
enum token_status fake_token_recv(socket_type, int *, gss_buffer_t, size_t,
                                  time_t);

/* The data, length, and flags sent by the last fake_token_send. */
extern char send_buffer[2048];
extern size_t send_length;
extern int send_flags;

/* The data, length, and flags returned by the next fake_token_recv. */
extern char recv_buffer[2048];
extern size_t recv_length;
extern int recv_flags;

/* If set to true, return timeout from the fake token functions. */
extern bool fail_timeout;

END_DECLS

#endif /* !TESTS_UTIL_FAKEWRITE_H */

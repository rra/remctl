/*
 * Testing interface to fake token functions.
 *
 * This header defines the interfaces to fake token functions used to test the
 * utility functions for sending and retrieving GSS-API tokens.
 *
 * Copyright 2018 Russ Allbery <eagle@eyrie.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

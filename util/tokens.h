/*
 * Prototypes for token handling routines.
 *
 * Originally written by Anton Ushakov
 * Extensive modifications by Russ Allbery <rra@stanford.edu>
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010
 *     Board of Trustees, Leland Stanford Jr. University
 *
 * See LICENSE for licensing terms.
 */

#ifndef UTIL_TOKENS_H
#define UTIL_TOKENS_H 1

#include <config.h>
#include <portable/gssapi.h>
#include <portable/macros.h>
#include <portable/socket.h>
#include <sys/types.h>

/* Token types and flags. */
enum token_flags {
    TOKEN_NOOP          = (1 << 0),
    TOKEN_CONTEXT       = (1 << 1),
    TOKEN_DATA          = (1 << 2),
    TOKEN_MIC           = (1 << 3),
    TOKEN_CONTEXT_NEXT  = (1 << 4),
    TOKEN_SEND_MIC      = (1 << 5),
    TOKEN_PROTOCOL      = (1 << 6)
};

/* Failure return codes from token_send and token_recv. */
enum token_status {
    TOKEN_OK = 0,
    TOKEN_FAIL_SYSTEM  = -1,    /* System call failed, error in errno */
    TOKEN_FAIL_SOCKET  = -2,    /* Socket call failed, error in socket_errno */
    TOKEN_FAIL_INVALID = -3,    /* Invalid token from remote site */
    TOKEN_FAIL_LARGE   = -4,    /* Token data exceeds max length */
    TOKEN_FAIL_EOF     = -5,    /* Unexpected end of file while reading */
    TOKEN_FAIL_GSSAPI  = -6     /* GSS-API failure {en,de}crypting token */
};

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/*
 * Sending and receiving tokens.  Do not use gss_release_buffer to free the
 * token returned by token_recv; this will cause crashes on Windows.  Call
 * free on the value member instead.
 */
enum token_status token_send(socket_type, int flags, gss_buffer_t);
enum token_status token_recv(socket_type, int *flags, gss_buffer_t,
                             size_t max);

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_GSS_ERRORS_H */

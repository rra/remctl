/*
 * Prototypes for GSS token handling routines.
 *
 * Originally written by Anton Ushakov
 * Extensive modifications by Russ Allbery <rra@stanford.edu>
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010
 *     Board of Trustees, Leland Stanford Jr. University
 *
 * See README for licensing terms.
 */

#ifndef UTIL_GSS_TOKENS_H
#define UTIL_GSS_TOKENS_H 1

#include <config.h>
#include <portable/gssapi.h>
#include <portable/macros.h>
#include <portable/socket.h>
#include <util/tokens.h>

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/*
 * Sending and receiving tokens with a GSS-API protection layer applied.  Do
 * not use gss_release_buffer to free the token returned by token_recv; this
 * will cause crashes on Windows.  Call free on the value member instead.  On
 * a GSS-API failure, the major and minor status are returned in the final two
 * arguments.
 */
enum token_status token_send_priv(socket_type, gss_ctx_id_t, int flags,
                                  gss_buffer_t, OM_uint32 *, OM_uint32 *);
enum token_status token_recv_priv(socket_type, gss_ctx_id_t, int *flags,
                                  gss_buffer_t, size_t max, OM_uint32 *,
                                  OM_uint32 *);

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_GSS_TOKENS_H */

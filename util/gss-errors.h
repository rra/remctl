/*
 * Prototypes for shared GSS-API error handling code.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2007, 2010
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * See LICENSE for licensing terms.
 */

#ifndef UTIL_GSS_ERRORS_H
#define UTIL_GSS_ERRORS_H 1

#include <config.h>
#include <portable/gssapi.h>
#include <portable/macros.h>

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/*
 * Convert a GSS-API error code pair into a human-readable string.  Returns a
 * newly allocated string that the caller must free.
 */
char *gssapi_error_string(const char *prefix, OM_uint32, OM_uint32)
    __attribute__((__malloc__, __nonnull__));

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_GSS_ERRORS_H */

/*
 * Prototypes for string concatenation with dynamic memory allocation.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * This work is hereby placed in the public domain by its author.
 */

#ifndef UTIL_CONCAT_H
#define UTIL_CONCAT_H 1

#include <config.h>
#include <portable/macros.h>

BEGIN_DECLS

/* Default to a hidden visibility for all util functions. */
#pragma GCC visibility push(hidden)

/* Concatenate NULL-terminated strings into a newly allocated string. */
char *concat(const char *first, ...)
    __attribute__((__malloc__, __nonnull__(1)));

/*
 * Given a base path and a file name, create a newly allocated path string.
 * The name will be appended to base with a / between them.  Exceptionally, if
 * name begins with a slash, it will be strdup'd and returned as-is.
 */
char *concatpath(const char *base, const char *name)
    __attribute__((__malloc__, __nonnull__(2)));

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* UTIL_CONCAT_H */

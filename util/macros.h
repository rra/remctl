/*
 * Some standard helpful macros.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * This work is hereby placed in the public domain by its author.
 */

#ifndef UTIL_MACROS_H
#define UTIL_MACROS_H 1

#include <config.h>
#include <portable/macros.h>

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))

#endif /* UTIL_MACROS_H */

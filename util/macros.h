/*
 * Some standard helpful macros.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008-2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef UTIL_MACROS_H
#define UTIL_MACROS_H 1

#include <portable/macros.h>

/*
 * Used for iterating through arrays.  ARRAY_SIZE returns the number of
 * elements in the array (useful for a < upper bound in a for loop) and
 * ARRAY_END returns a pointer to the element past the end (ISO C99 makes it
 * legal to refer to such a pointer as long as it's never dereferenced).
 */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define ARRAY_END(array)  (&(array)[ARRAY_SIZE(array)])

/* Used to name the elements of the array passed to pipe. */
#define PIPE_READ         0
#define PIPE_WRITE        1

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED            __attribute__((__unused__))

#endif /* UTIL_MACROS_H */

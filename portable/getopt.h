/*
 * Replacement implementation of getopt.
 *
 * This is a replacement implementation for getopt based on the my_getopt
 * distribution by Benjamin Sittler.  Only the getopt interface is included,
 * since remctl doesn't use GNU long options, and the code has been rearranged
 * and reworked somewhat to fit with my coding style.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2008 Russ Allbery <eagle@eyrie.org>
 * Copyright 1997, 2000-2002 Benjamin Sittler
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

#ifndef PORTABLE_GETOPT_H
#define PORTABLE_GETOPT_H 1

#include <config.h>
#include <portable/macros.h>

/* Skip this entire file if we already have a getopt. */
#if !HAVE_GETOPT

BEGIN_DECLS

/* Default to a hidden visibility for all portability functions. */
#    pragma GCC visibility push(hidden)

/*
 * The current element in the argv array or, if getopt returns -1, the index
 * of the first non-option argument.
 */
extern int optind;

/* Set to zero to suppress error messages to stderr for unknown options. */
extern int opterr;

/* Holds the option character seen for unrecognized options. */
extern int optopt;

/* The argument to an option. */
extern char *optarg;

/* The primary interface.  Call repeatedly to return each option. */
int getopt(int argc, char *argv[], const char *opts);

/* Undo default visibility change. */
#    pragma GCC visibility pop

END_DECLS

#endif /* !HAVE_GETOPT */
#endif /* !PORTABLE_GETOPT_H */

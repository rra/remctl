/*
 * Utility functions for tests that use subprocesses.
 *
 * Copyright 2009, 2010 Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * See LICENSE for licensing terms.
 */

#ifndef TAP_PROCESS_H
#define TAP_PROCESS_H 1

#include <config.h>
#include <portable/macros.h>

BEGIN_DECLS

/*
 * Run a function in a subprocess and check the exit status and expected
 * output (stdout and stderr combined) against the provided values.  Expects
 * the function to always exit (not die from a signal).
 *
 * This reports as three separate tests: whether the function exited rather
 * than was killed, whether the exit status was correct, and whether the
 * output was correct.
 */
typedef void (*test_function_type)(void);
void is_function_output(test_function_type, int status, const char *output,
                        const char *format, ...)
    __attribute__((__format__(printf, 4, 5)));

END_DECLS

#endif /* TAP_PROCESS_H */

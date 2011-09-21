/*
 * Utility functions for tests that use Kerberos.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2009, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
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
 */

#ifndef TAP_KERBEROS_H
#define TAP_KERBEROS_H 1

#include <config.h>
#include <portable/macros.h>

BEGIN_DECLS

/*
 * Set up Kerberos, returning the test principal.  If there is no principal in
 * tests/data/test.principal or no keytab in tests/data/test.keytab, return
 * NULL.  Otherwise, on failure, calls bail().
 *
 * kerberos_cleanup will be set up to run from an atexit() handler.  This
 * means that any child processes that should not remove the Kerberos ticket
 * cache should call _exit instead of exit.
 *
 * The principal will be automatically freed when kerberos_cleanup is called
 * or if kerberos_setup is called again.  The caller doesn't need to worry
 * about it.
 */
const char *kerberos_setup(void)
    __attribute__((__malloc__));

/*
 * Clean up at the end of a test.  This is registered as an atexit handler,
 * so normally never needs to be called explicitly.
 */
void kerberos_cleanup(void);

END_DECLS

#endif /* !TAP_MESSAGES_H */

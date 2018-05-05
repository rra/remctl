/*
 * Error handling for Kerberos.
 *
 * Provides versions of die and warn that take a Kerberos context and a
 * Kerberos error code and append the Kerberos error message to the provided
 * formatted message.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2010, 2013
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
 *
 * SPDX-License-Identifier: MIT
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/system.h>

#include <util/messages.h>
#include <util/messages-krb5.h>
#include <util/xmalloc.h>


/*
 * Report a Kerberos error and exit.
 */
void
die_krb5(krb5_context ctx, krb5_error_code code, const char *format, ...)
{
    const char *k5_msg = NULL;
    char *message;
    va_list args;

    if (ctx != NULL)
        k5_msg = krb5_get_error_message(ctx, code);
    va_start(args, format);
    xvasprintf(&message, format, args);
    va_end(args);
    if (k5_msg == NULL)
        die("%s", message);
    else
        die("%s: %s", message, k5_msg);
}


/*
 * Report a Kerberos error.
 */
void
warn_krb5(krb5_context ctx, krb5_error_code code, const char *format, ...)
{
    const char *k5_msg = NULL;
    char *message;
    va_list args;

    if (ctx != NULL)
        k5_msg = krb5_get_error_message(ctx, code);
    va_start(args, format);
    xvasprintf(&message, format, args);
    va_end(args);
    if (k5_msg == NULL)
        warn("%s", message);
    else
        warn("%s: %s", message, k5_msg);
    free(message);
    if (k5_msg != NULL)
        krb5_free_error_message(ctx, k5_msg);
}

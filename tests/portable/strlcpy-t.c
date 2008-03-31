/* $Id$
 *
 * strlcpy test suite.
 *
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * This code is derived from software contributed to the Internet Software
 * Consortium by Rich Salz.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>
#include <portable/system.h>

#include <tests/libtest.h>

size_t test_strlcpy(char *, const char *, size_t);


int
main(void)
{
    char buffer[10];

    test_init(23);

    ok_int(1, 3, test_strlcpy(buffer, "foo", sizeof(buffer)));
    ok_string(2, "foo", buffer);
    ok_int(3, 9, test_strlcpy(buffer, "hello wor", sizeof(buffer)));
    ok_string(4, "hello wor", buffer);
    ok_int(5, 10, test_strlcpy(buffer, "world hell", sizeof(buffer)));
    ok_string(6, "world hel", buffer);
    ok(7, buffer[9] == '\0');
    ok_int(8, 11, test_strlcpy(buffer, "hello world", sizeof(buffer)));
    ok_string(9, "hello wor", buffer);
    ok(10, buffer[9] == '\0');

    /* Make sure that with a size of 0, the destination isn't changed. */
    ok_int(11, 3, test_strlcpy(buffer, "foo", 0));
    ok_string(12, "hello wor", buffer);

    /* Now play with empty strings. */
    ok_int(13, 0, test_strlcpy(buffer, "", 0));
    ok_string(14, "hello wor", buffer);
    ok_int(15, 0, test_strlcpy(buffer, "", sizeof(buffer)));
    ok_string(16, "", buffer);
    ok_int(17, 3, test_strlcpy(buffer, "foo", 2));
    ok_string(18, "f", buffer);
    ok(19, buffer[1] == '\0');
    ok_int(20, 0, test_strlcpy(buffer, "", 1));
    ok(21, buffer[0] == '\0');

    /* Finally, check using strlcpy as strlen. */
    ok_int(22, 3, test_strlcpy(NULL, "foo", 0));
    ok_int(23, 11, test_strlcpy(NULL, "hello world", 0));

    return 0;
}

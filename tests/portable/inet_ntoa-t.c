/* $Id$ */
/* inet_ntoa test suite. */

/* Copyright (c) 2004, 2005, 2006, 2007
       by Internet Systems Consortium, Inc. ("ISC")
   Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
       2002, 2003 by The Internet Software Consortium and Rich Salz

   This code is derived from software contributed to the Internet Software
   Consortium by Rich Salz.

   Permission to use, copy, modify, and distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
   REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
   MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
   SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <config.h>
#include <system.h>

#include <netinet/in.h>

#include <tests/libtest.h>

const char *test_inet_ntoa(const struct in_addr);

static void
test_addr(int n, const char *expected, unsigned long addr)
{
    struct in_addr in;

    in.s_addr = htonl(addr);
    ok_string(n, expected, test_inet_ntoa(in));
}

int
main(void)
{
    test_init(5);

    test_addr(1,         "0.0.0.0", 0x0);
    test_addr(2,       "127.0.0.0", 0x7f000000UL);
    test_addr(3, "255.255.255.255", 0xffffffffUL);
    test_addr(4, "172.200.232.199", 0xacc8e8c7UL);
    test_addr(5,         "1.2.3.4", 0x01020304UL);

    return 0;
}

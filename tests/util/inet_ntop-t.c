/* $Id$ */
/* inet_ntop test suite. */

/* Copyright (c) 2004, 2005, 2006
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

#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <tests/libtest.h>

#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT EDOM
#endif

const char *test_inet_ntop(int, const void *, char *, socklen_t);

static int
test_addr(int n, const char *expected, unsigned long addr)
{
    struct in_addr in;
    char result[INET_ADDRSTRLEN];

    in.s_addr = htonl(addr);
    if (test_inet_ntop(AF_INET, &in, result, sizeof(result)) == NULL) {
        ok(n++, 0);
        printf("# cannot convert %lu: %s", addr, strerror(errno));
    } else
        ok(n++, 1);
    ok_string(n++, expected, result);
    return n;
}

int
main(void)
{
    int n;

    test_init(6 + 5 * 2);

    ok(1, test_inet_ntop(AF_UNIX, NULL, NULL, 0) == NULL);
    ok_int(2, EAFNOSUPPORT, errno);
    ok(3, test_inet_ntop(AF_INET, NULL, NULL, 0) == NULL);
    ok_int(4, ENOSPC, errno);
    ok(5, test_inet_ntop(AF_INET, NULL, NULL, 11) == NULL);
    ok_int(6, ENOSPC, errno);

    n = 7;
    n = test_addr(n,         "0.0.0.0", 0x0);
    n = test_addr(n,       "127.0.0.0", 0x7f000000UL);
    n = test_addr(n, "255.255.255.255", 0xffffffffUL);
    n = test_addr(n, "172.200.232.199", 0xacc8e8c7UL);
    n = test_addr(n,         "1.2.3.4", 0x01020304UL);

    return 0;
}

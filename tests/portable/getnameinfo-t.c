/* $Id$ */
/* getnameinfo test suite. */

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
#include <portable/socket.h>

#include <tests/libtest.h>
#include <util/util.h>

int test_getnameinfo(const struct sockaddr *, socklen_t, char *, socklen_t,
                     char *, socklen_t, int);

/* Linux doesn't provide EAI_OVERFLOW, so make up our own for testing. */
#ifndef EAI_OVERFLOW
# define EAI_OVERFLOW 10
#endif

int
main(void)
{
    char node[256], service[256];
    struct sockaddr_in sin;
    struct sockaddr *sa = (struct sockaddr *) &sin;
    int status;
    struct hostent *hp;
    struct servent *serv;
    char *name;

    test_init(26);

    /* Test the easy stuff that requires no assumptions.  Hopefully everyone
       has nntp, exec, and biff as services. */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(119);
    inet_aton("10.20.30.40", &sin.sin_addr);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, NULL, 0, 0);
    ok_int(1, EAI_NONAME, status);
    status = test_getnameinfo(sa, sizeof(sin), node, 0, service, 0, 0);
    ok_int(2, EAI_NONAME, status);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), 0);
    ok_int(3, 0, status);
    serv = getservbyname("nntp", "tcp");
    if (serv == NULL)
        skip_block(4, 5, "nntp service not found");
    else {
        ok_string(4, "nntp", service);
        service[0] = '\0';
        status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 1, 0);
        ok_int(5, EAI_OVERFLOW, status);
        status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 4, 0);
        ok_int(6, EAI_OVERFLOW, status);
        status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service, 5, 0);
        ok_int(7, 0, status);
        ok_string(8, "nntp", service);
    }
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), NI_NUMERICSERV);
    ok_int(9, 0, status);
    ok_string(10, "119", service);
    sin.sin_port = htons(512);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), 0);
    ok_int(11, 0, status);
    serv = getservbyname("exec", "tcp");
    if (serv == NULL)
        skip(12, "exec service not found");
    else
        ok_string(12, "exec", service);
    status = test_getnameinfo(sa, sizeof(sin), NULL, 0, service,
                              sizeof(service), NI_DGRAM);
    ok_int(13, 0, status);
    serv = getservbyname("biff", "udp");
    if (serv == NULL)
        skip(14, "biff service not found");
    else
        ok_string(14, "biff", service);
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0,
                              NI_NUMERICHOST);
    ok_int(15, 0, status);
    ok_string(16, "10.20.30.40", node);
    node[0] = '\0';
    status = test_getnameinfo(sa, sizeof(sin), node, 1, NULL, 0,
                              NI_NUMERICHOST);
    ok_int(17, EAI_OVERFLOW, status);
    status = test_getnameinfo(sa, sizeof(sin), node, 11, NULL, 0,
                              NI_NUMERICHOST);
    ok_int(18, EAI_OVERFLOW, status);
    status = test_getnameinfo(sa, sizeof(sin), node, 12, NULL, 0,
                              NI_NUMERICHOST);
    ok_int(19, 0, status);
    ok_string(20, "10.20.30.40", node);

    /* Okay, now it gets annoying.  Do a forward and then reverse lookup of
       some well-known host and make sure that getnameinfo returns the same
       results.  This may need to be skipped. */
    hp = gethostbyname("www.isc.org");
    if (hp == NULL)
        skip_block(21, 2, "cannot look up www.isc.org");
    else {
        memcpy(&sin.sin_addr, hp->h_addr, sizeof(sin.sin_addr));
        hp = gethostbyaddr(&sin.sin_addr, sizeof(sin.sin_addr), AF_INET);
        if (hp == NULL || strchr(hp->h_name, '.') == NULL)
            skip_block(21, 2, "cannot reverse-lookup www.isc.org");
        else {
            name = xstrdup(hp->h_name);
            status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node),
                                      NULL, 0, 0);
            ok_int(21, 0, status);
            ok_string(22, name, node);
            free(name);
        }
    }

    /* Hope that no one is weird enough to put 0.0.0.0 into DNS. */
    inet_aton("0.0.0.0", &sin.sin_addr);
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0, 0);
    ok_int(23, 0, status);
    ok_string(24, "0.0.0.0", node);
    node[0] = '\0';
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0,
                              NI_NAMEREQD);
    ok_int(25, EAI_NONAME, status);

    sin.sin_family = AF_UNIX;
    status = test_getnameinfo(sa, sizeof(sin), node, sizeof(node), NULL, 0, 0);
    ok_int(26, EAI_FAMILY, status);

    return 0;
}

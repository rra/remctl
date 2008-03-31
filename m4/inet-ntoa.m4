dnl inet-ntoa.m4 -- Check for a working inet_ntoa.
dnl $Id$
dnl
dnl Check whether inet_ntoa is present and working.  Since calling inet_ntoa
dnl involves passing small structs on the stack, present and working versions
dnl may still not function with gcc on some platforms (such as IRIX).
dnl Provides RRA_FUNC_INET_NTOA and defines HAVE_INET_NTOA if inet_ntoa is
dnl present and working.
dnl
dnl Copyright 2008 Board of Trustees, Leland Stanford Jr. University
dnl Copyright (c) 2004, 2005, 2006, 2007
dnl     by Internet Systems Consortium, Inc. ("ISC")
dnl Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
dnl     2002, 2003 by The Internet Software Consortium and Rich Salz
dnl
dnl This code is derived from software contributed to the Internet Software
dnl Consortium by Rich Salz.
dnl
dnl Permission to use, copy, modify, and distribute this software for any
dnl purpose with or without fee is hereby granted, provided that the above
dnl copyright notice and this permission notice appear in all copies.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
dnl REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
dnl SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
dnl OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

dnl Source used by RRA_FUNC_INET_NTOA.
define([_RRA_FUNC_INET_NTOA_SOURCE],
[#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

int
main(void)
{
    struct in_addr in;
    in.s_addr = htonl(0x7f000000L);
    return (strcmp(inet_ntoa(in), "127.0.0.0") == 0) ? 0 : 1;
}])

dnl The public macro.
AC_DEFUN([RRA_FUNC_INET_NTOA],
[AC_CACHE_CHECK(for working inet_ntoa, rra_cv_func_inet_ntoa_works,
[AC_TRY_RUN(_RRA_FUNC_INET_NTOA_SOURCE(),
    [rra_cv_func_inet_ntoa_works=yes],
    [rra_cv_func_inet_ntoa_works=no],
    [rra_cv_func_inet_ntoa_works=no])])
AS_IF([test "$rra_cv_func_inet_ntoa_works" = yes],
    [AC_DEFINE([HAVE_INET_NTOA], 1,
        [Define if your system has a working inet_ntoa function.])],
    [AC_LIBOBJ([inet_ntoa])])])

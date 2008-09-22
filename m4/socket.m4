dnl socket.m4 -- Various checks for socket support and macros.
dnl
dnl This is a collection of various Autoconf macros for checking networking
dnl and socket properties.  The macros provided are:
dnl
dnl     RRA_MACRO_IN6_ARE_ADDR_EQUAL
dnl     RRA_MACRO_SA_LEN
dnl
dnl Most of them use a separate internal source macro to make the code easier
dnl to read.
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

dnl Source used by INN_IN6_EQ_BROKEN.  Test borrowed from a bug report by
dnl tmoestl@gmx.net for glibc.
AC_DEFUN([_RRA_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE],
[#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
main (void)
{
    struct in6_addr a;
    struct in6_addr b;

    inet_pton(AF_INET6, "fe80::1234:5678:abcd", &a);
    inet_pton(AF_INET6, "fe80::1234:5678:abcd", &b);
    return IN6_ARE_ADDR_EQUAL(&a, &b) ? 0 : 1;
}])

dnl Check whether the IN6_ARE_ADDR_EQUAL macro is broken (like glibc 2.1.3) or
dnl missing.
AC_DEFUN([RRA_MACRO_IN6_ARE_ADDR_EQUAL],
[AC_CACHE_CHECK([whether IN6_ARE_ADDR_EQUAL macro is broken],
    [rra_cv_in6_are_addr_equal_broken],
    [AC_TRY_RUN(_RRA_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE,
        rra_cv_in6_are_addr_equal_broken=no,
        rra_cv_in6_are_addr_equal_broken=yes,
        rra_cv_in6_are_addr_equal_broken=yes)])
AS_IF([test x"$rra_cv_in6_are_addr_equal_broken" = xyes],
    [AC_DEFINE([HAVE_BROKEN_IN6_ARE_ADDR_EQUAL], 1,
        [Define if your IN6_ARE_ADDR_EQUAL macro is broken.])])])

dnl Check whether the SA_LEN macro is available.  This should give the length
dnl of a struct sockaddr regardless of type.
AC_DEFUN([RRA_MACRO_SA_LEN],
[AC_CACHE_CHECK([for SA_LEN macro], [rra_cv_sa_len_macro],
[AC_TRY_LINK(
[#include <sys/types.h>
#include <sys/socket.h>],
    [struct sockaddr sa; int x = SA_LEN(&sa);],
    [rra_cv_sa_len_macro=yes],
    [rra_cv_sa_len_macro=no])])
AS_IF([test "$rra_cv_sa_len_macro" = yes],
    [AC_DEFINE([HAVE_SA_LEN], 1,
        [Define if <sys/socket.h> defines the SA_LEN macro])])])

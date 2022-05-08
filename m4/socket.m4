dnl Various checks for socket support and macros.
dnl
dnl This is a collection of various Autoconf macros for checking networking
dnl and socket properties.  The macros provided are:
dnl
dnl     RRA_FUNC_GETADDRINFO_ADDRCONFIG
dnl     RRA_MACRO_IN6_ARE_ADDR_EQUAL
dnl
dnl They use a separate internal source macro to make the code easier to read.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2017, 2020-2021 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2008-2009, 2011
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl Copyright 2004-2009 Internet Systems Consortium, Inc. ("ISC")
dnl Copyright 1991, 1994-2003 The Internet Software Consortium and Rich Salz
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
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
dnl IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl SPDX-License-Identifier: ISC

dnl Source used by RRA_FUNC_GETADDRINFO_ADDRCONFIG.
AC_DEFUN([_RRA_FUNC_GETADDRINFO_ADDRCONFIG_SOURCE], [[
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

int
main(void) {
    struct addrinfo hints, *ai;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    return (getaddrinfo("localhost", NULL, &hints, &ai) != 0);
}
]])

dnl Check whether the AI_ADDRCONFIG flag works properly with getaddrinfo.
dnl If so, set HAVE_GETADDRINFO_ADDRCONFIG.
AC_DEFUN([RRA_FUNC_GETADDRINFO_ADDRCONFIG],
[AC_CACHE_CHECK([for working AI_ADDRCONFIG flag],
    [rra_cv_func_getaddrinfo_addrconfig_works],
    [AC_RUN_IFELSE([AC_LANG_SOURCE([_RRA_FUNC_GETADDRINFO_ADDRCONFIG_SOURCE])],
        [rra_cv_func_getaddrinfo_addrconfig_works=yes],
        [rra_cv_func_getaddrinfo_addrconfig_works=no],
        [rra_cv_func_getaddrinfo_addrconfig_works=no])])
 AS_IF([test x"$rra_cv_func_getaddrinfo_addrconfig_works" = xyes],
    [AC_DEFINE([HAVE_GETADDRINFO_ADDRCONFIG], 1,
        [Define if the AI_ADDRCONFIG flag works with getaddrinfo.])])])

dnl Source used by RRA_MACRO_IN6_ARE_ADDR_EQUAL.  Test borrowed from a bug
dnl report by tmoestl@gmx.net for glibc.
AC_DEFUN([_RRA_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE], [[
#include <string.h>
#include <sys/types.h>
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
}
]])

dnl Check whether the IN6_ARE_ADDR_EQUAL macro is broken (like glibc 2.1.3) or
dnl missing.
AC_DEFUN([RRA_MACRO_IN6_ARE_ADDR_EQUAL],
[AC_CACHE_CHECK([whether IN6_ARE_ADDR_EQUAL macro is broken],
    [rra_cv_in6_are_addr_equal_broken],
    [AC_RUN_IFELSE([AC_LANG_SOURCE([_RRA_MACRO_IN6_ARE_ADDR_EQUAL_SOURCE])],
        [rra_cv_in6_are_addr_equal_broken=no],
        [rra_cv_in6_are_addr_equal_broken=yes],
        [rra_cv_in6_are_addr_equal_broken=yes])])
 AS_IF([test x"$rra_cv_in6_are_addr_equal_broken" = xyes],
    [AC_DEFINE([HAVE_BROKEN_IN6_ARE_ADDR_EQUAL], 1,
        [Define if your IN6_ARE_ADDR_EQUAL macro is broken.])])])

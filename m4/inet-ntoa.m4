dnl Check for a working inet_ntoa.
dnl
dnl Check whether inet_ntoa is present and working.  Since calling inet_ntoa
dnl involves passing small structs on the stack, present and working versions
dnl may still not function with gcc on some platforms (such as IRIX).
dnl Provides RRA_FUNC_INET_NTOA and defines HAVE_INET_NTOA if inet_ntoa is
dnl present and working.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 1999-2001, 2003 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2008-2009
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Source used by RRA_FUNC_INET_NTOA.
AC_DEFUN([_RRA_FUNC_INET_NTOA_SOURCE], [[
#include <sys/types.h>
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
}
]])

dnl The public macro.
AC_DEFUN([RRA_FUNC_INET_NTOA],
[AC_CACHE_CHECK(for working inet_ntoa, rra_cv_func_inet_ntoa_works,
    [AC_RUN_IFELSE([AC_LANG_SOURCE([_RRA_FUNC_INET_NTOA_SOURCE])],
        [rra_cv_func_inet_ntoa_works=yes],
        [rra_cv_func_inet_ntoa_works=no],
        [rra_cv_func_inet_ntoa_works=no])])
 AS_IF([test x"$rra_cv_func_inet_ntoa_works" = xyes],
    [AC_DEFINE([HAVE_INET_NTOA], 1,
        [Define if your system has a working inet_ntoa function.])],
    [AC_LIBOBJ([inet_ntoa])])])

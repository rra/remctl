dnl Process this file with autoconf to produce a configure script.
dnl $Id$
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2002, 2003, 2004, 2005, 2006
dnl     Board of Trustees, Leland Stanford Jr. University
dnl
dnl See README for licensing terms.

AC_REVISION($Revision$)
AC_PREREQ(2.57)
AC_INIT([remctl], [2.0], [rra@stanford.edu])
AC_CONFIG_AUX_DIR([tools])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_RANLIB
AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket], ,
    [AC_CHECK_LIB([nsl], [socket],
        [LIBS="-lnsl -lsocket $LIBS"], , [-lsocket])])
RRA_LIB_KRB5([gssapi], [false])
AC_CHECK_HEADERS([inttypes.h stdint.h sys/bitypes.h sys/select.h])
AC_CHECK_DECLS([snprintf, vsnprintf])
RRA_C_C99_VAMACROS
RRA_C_GNU_VAMACROS
AC_CHECK_TYPES([long long])
RRA_FUNC_SNPRINTF
AC_CHECK_FUNCS([setrlimit])
AC_CHECK_FUNC([asprintf], [], [AC_LIBOBJ([util/asprintf])])
AC_CHECK_FUNC([setenv], [], [AC_LIBOBJ([util/setenv])])
AC_CHECK_FUNC([strlcat], [], [AC_LIBOBJ([util/strlcat])])
AC_CHECK_FUNC([strlcpy], [], [AC_LIBOBJ([util/strlcpy])])
AC_TYPE_SIGNAL

AC_CONFIG_HEADER([config.h])
AC_OUTPUT([Makefile])

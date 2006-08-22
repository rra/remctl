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
AC_INIT([remctl], [2.1], [rra@stanford.edu])
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
AC_CHECK_TYPE([socklen_t], ,
    [AC_DEFINE([socklen_t], [int],
        [Define to int if <sys/socket.h> does not define.])],
[#include <sys/types.h>
#include <sys/socket.h>])
RRA_FUNC_SNPRINTF
AC_CHECK_FUNCS([setrlimit])
AC_CHECK_FUNCS([asprintf], [], [AC_LIBOBJ([util/asprintf])])
AC_CHECK_FUNCS([inet_ntop], [], [AC_LIBOBJ([util/inet_ntop])])
AC_CHECK_FUNCS([setenv], [], [AC_LIBOBJ([util/setenv])])
AC_CHECK_FUNCS([strlcat], [], [AC_LIBOBJ([util/strlcat])])
AC_CHECK_FUNCS([strlcpy], [], [AC_LIBOBJ([util/strlcpy])])
AC_TYPE_SIGNAL

dnl Needed to get prototypes for functions like asprintf on Linux.
AC_DEFINE([_GNU_SOURCE], [1], [Define to 1 on Linux to get full prototypes.])

dnl In order to run the test suite out of the build directory when doing a
dnl VPATH build, we need to copy some of the test suite files that aren't
dnl otherwise touched during the build process into the build directory.
AC_CONFIG_COMMANDS([tests/TESTS],
    [if test x"$ac_srcdir" != x. && test x"$ac_srcdir" != x ; then
        for f in TESTS client/remctl-t data/acl-nonexistant data/cmd-env \
                 data/cmd-hello data/cmd-status data/simple.conf \
                 util/xmalloc-t ; do
            (cd tests && cp "$ac_srcdir/$f" "$f")
        done
     fi], [LN_S="$LN_S"])

AC_CONFIG_HEADER([config.h])
AC_OUTPUT([Makefile])

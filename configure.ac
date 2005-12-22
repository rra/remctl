dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_REVISION($Revision$)
AC_PREREQ(2.57)
AC_INIT([remctl], [1.11], [rra@stanford.edu])

m4_include([m4/krb5.m4])
m4_include([m4/snprintf.m4])

AC_PROG_CC
AC_PROG_INSTALL
AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket], ,
    [AC_CHECK_LIB([nsl], [socket],
        [LIBS="-lnsl -lsocket $LIBS"], , [-lsocket])])
RRA_LIB_KRB5([gssapi], [false])
AC_CHECK_TYPES([long long])
RRA_FUNC_SNPRINTF
AC_TYPE_SIGNAL

AC_CONFIG_HEADER(config.h)
AC_OUTPUT(Makefile)

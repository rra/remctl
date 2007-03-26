dnl Process this file with autoconf to produce a configure script.
dnl $Id$
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2002, 2003, 2004, 2005, 2006, 2007
dnl     Board of Trustees, Leland Stanford Jr. University
dnl
dnl See README for licensing terms.

AC_REVISION([$Revision$])
AC_PREREQ([2.61])
AC_INIT([remctl], [2.7], [rra@stanford.edu])
AC_CONFIG_AUX_DIR([tools])
AC_CONFIG_LIBOBJ_DIR([portable])
AM_INIT_AUTOMAKE([1.10])
AM_MAINTAINER_MODE

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket], ,
    [AC_CHECK_LIB([nsl], [socket],
        [LIBS="-lnsl -lsocket $LIBS"], , [-lsocket])])
RRA_LIB_KRB5([gssapi], [false])
AC_CHECK_HEADERS([inttypes.h stdint.h sys/bitypes.h sys/select.h])
AC_CHECK_DECLS([snprintf, vsnprintf])
AC_CHECK_DECLS([inet_aton, inet_ntoa], , ,
    [#include <sys/types.h>
     #include <netinet/in.h>
     #include <arpa/inet.h>])
RRA_C_C99_VAMACROS
RRA_C_GNU_VAMACROS
AC_CHECK_MEMBERS([struct sockaddr.sa_len], , ,
    [#include <sys/types.h>
     #include <sys/socket.h>])
AC_CHECK_TYPES([long long])
AC_CHECK_TYPE([socklen_t], ,
    [AC_DEFINE([socklen_t], [int],
        [Define to int if <sys/socket.h> does not define.])],
    [#include <sys/types.h>
     #include <sys/socket.h>])
AC_CHECK_TYPES([struct sockaddr_in6],
    [AC_DEFINE([HAVE_INET6], [1],
        [Define to 1 if IPv6 library interfaces are available.])], ,
    [#include <sys/types.h>
     #include <netinet/in.h>])
AC_CHECK_TYPES([struct sockaddr_storage],
    [AC_CHECK_MEMBERS([struct sockaddr_storage.ss_family], , ,
        [#include <sys/types.h>
         #include <sys/socket.h>])], ,
    [#include <sys/types.h>
     #include <sys/socket.h>])
RRA_MACRO_IN6_ARE_ADDR_EQUAL
RRA_MACRO_SA_LEN
RRA_FUNC_INET_NTOA
RRA_FUNC_SNPRINTF
AC_CHECK_FUNCS([setrlimit])
AC_REPLACE_FUNCS([asprintf getaddrinfo getnameinfo inet_aton inet_ntop \
                  setenv strlcat strlcpy])
AC_TYPE_SIGNAL

dnl Needed to get prototypes for functions like asprintf on Linux.
AC_DEFINE([_GNU_SOURCE], [1], [Define to 1 on Linux to get full prototypes.])

dnl In order to run the test suite out of the build directory when doing a
dnl VPATH build, we need to copy some of the test suite files that aren't
dnl otherwise touched during the build process into the build directory.
AC_CONFIG_COMMANDS([tests/TESTS],
    [if test x"$ac_srcdir" != x. && test x"$ac_srcdir" != x ; then
        AS_MKDIR_P([tests/data/acls])
        for f in TESTS client/remctl-t data/acl-simple data/acl-bad-include \
                 data/acl-bad-syntax data/acl-nonexistant data/acl-recursive \
                 data/acl-too-long data/acls/valid data/acls/valid-2 \
                 data/cmd-argv data/cmd-env data/cmd-hello data/cmd-status \
                 data/conf-simple data/conf-test util/xmalloc-t ; do
            (cd tests && cp "$ac_srcdir/$f" "$f")
        done
     fi])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADER([config.h])
AC_OUTPUT

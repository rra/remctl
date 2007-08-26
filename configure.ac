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
AC_INIT([remctl], [2.9], [rra@stanford.edu])
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
AC_CHECK_TYPE([sig_atomic_t], ,
    [AC_DEFINE([sig_atomic_t], [int],
        [Define to int if <signal.h> does not define.])],
    [#include <sys/types.h>
     #include <signal.h>])
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
AC_CHECK_FUNCS([setrlimit setsid])
AC_REPLACE_FUNCS([asprintf daemon getaddrinfo getnameinfo inet_aton \
                  inet_ntop setenv strlcat strlcpy])
AC_TYPE_SIGNAL

dnl Needed to get prototypes for functions like asprintf on Linux.
AC_DEFINE([_GNU_SOURCE], [1], [Define to 1 on Linux to get full prototypes.])

dnl If and only if we're on a GNU system, use mapfile to do symbol versioning
dnl of the libremctl library.  libtool doesn't know how to do this itself.
dnl This is supported on Solaris as well but we need to know if we're
dnl the GNU linker or the Solaris linker.
case "$host" in
*-gnu*)
    VERSION_LDFLAGS='-Wl,--version-script=${srcdir}/client/mapfile'
    ;;
*)
    VERSION_LDFLAGS=""
    ;;
esac
AC_SUBST([VERSION_LDFLAGS])

dnl Whether to build the Perl bindings.  Put this late so that it shows up
dnl near the bottom of the --help output.
build_perl=
AC_ARG_ENABLE([perl],
    AC_HELP_STRING([--enable-perl], [Build Perl libremctl bindings]),
    [if test x"$enableval" = xyes ; then
        build_perl=yes
     fi])
AM_CONDITIONAL([BUILD_PERL], [test x"$build_perl" = xyes])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([tests/client/pod-t], [chmod +x tests/client/pod-t])
if test x"$build_perl" = xyes ; then
    AC_CONFIG_FILES([perl/Makefile.PL])
fi
AC_CONFIG_HEADER([config.h])
AC_CONFIG_COMMANDS([docs], [test -d docs || mkdir docs])
AC_OUTPUT

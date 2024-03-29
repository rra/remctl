dnl Process this file with autoconf to produce a configure script.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2015-2016, 2018-2020, 2022 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2016 Dropbox, Inc.
dnl Copyright 2002-2008, 2010-2014
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl SPDX-License-Identifier: MIT

AC_PREREQ([2.64])
AC_INIT([remctl], [3.18], [eagle@eyrie.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_LIBOBJ_DIR([portable])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 check-news dist-xz foreign silent-rules subdir-objects
    -Wall -Werror])
AM_MAINTAINER_MODE

dnl Detect unexpanded macros.
m4_pattern_forbid([^PKG_])
m4_pattern_forbid([^_?RRA_])

dnl AM_PROG_AR is required for Automake 1.12 by Libtool but not defined at all
dnl (or needed) in Automake 1.11.  Work around this bug.
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
RRA_PROG_CC_WARNINGS_FLAGS
AC_SYS_LARGEFILE
AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT
RRA_LD_VERSION_SCRIPT

dnl Support installation of systemd unit files.
RRA_WITH_SYSTEMD_UNITDIR

dnl Find a sudo binary for the sudo remctld configuration option.
AC_ARG_VAR([PATH_SUDO], [Path to sudo for executing commands])
AC_PATH_PROG([PATH_SUDO], [sudo])

dnl Only used for the test suite.
AC_ARG_VAR([PATH_FAKEROOT], [Path to fakeroot for the test suite])
AC_PATH_PROG([PATH_FAKEROOT], [fakeroot])
AC_ARG_VAR([PATH_VALGRIND], [Path to valgrind for the test suite])
AC_PATH_PROG([PATH_VALGRIND], [valgrind])

dnl Probes for networking libraries.
AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket], [],
    [AC_CHECK_LIB([nsl], [socket], [LIBS="-lnsl -lsocket $LIBS"], [],
        [-lsocket])])

dnl Kerberos portability checks.
RRA_LIB_KRB5_OPTIONAL
AS_IF([test x"$rra_use_KRB5" = xtrue],
    [RRA_LIB_KRB5_SWITCH
     AC_CHECK_FUNCS([krb5_free_default_realm \
         krb5_get_init_creds_opt_alloc \
         krb5_get_init_creds_opt_set_anonymous \
         krb5_get_init_creds_opt_set_default_flags \
         krb5_get_init_creds_opt_set_out_ccache \
         krb5_init_creds_set_password \
         krb5_principal_get_realm \
         krb5_xfree])
     AC_CHECK_FUNCS([krb5_get_init_creds_opt_free],
        [RRA_FUNC_KRB5_GET_INIT_CREDS_OPT_FREE_ARGS])
     AC_CHECK_DECLS([krb5_kt_free_entry], [], [], [RRA_INCLUDES_KRB5])
     AC_LIBOBJ([krb5-extra])
     RRA_LIB_KRB5_RESTORE])
AM_CONDITIONAL([HAVE_KRB5], [test x"$rra_use_KRB5" = xtrue])

dnl GSS-API portability checks.
RRA_LIB_GSSAPI
RRA_LIB_GSSAPI_SWITCH
AC_CHECK_DECLS([GSS_KRB5_MECHANISM], [],
   [AC_CHECK_DECLS([gss_mech_krb5], [],
       [AC_LIBOBJ([gssapi-mech])], [RRA_INCLUDES_GSSAPI])],
   [RRA_INCLUDES_GSSAPI])
AC_CHECK_FUNCS([gss_krb5_ccache_name gss_krb5_import_cred gss_oid_equal])
RRA_LIB_GSSAPI_RESTORE

dnl Check for libevent, used by the server.
RRA_LIB_EVENT
RRA_LIB_EVENT_SWITCH
AC_CHECK_HEADERS([event2/event.h])
AC_CHECK_TYPES([bufferevent_data_cb, bufferevent_event_cb, event_callback_fn,
                evutil_socket_t],
    [], [], [RRA_INCLUDES_EVENT])
AC_CHECK_FUNCS([bufferevent_get_input \
    bufferevent_read_buffer \
    bufferevent_socket_new \
    evbuffer_get_length \
    event_base_got_break \
    event_base_loopbreak \
    event_free \
    event_new \
    event_set_fatal_callback \
    libevent_global_shutdown])
RRA_LIB_EVENT_RESTORE

dnl Check for the systemd daemon support library.
RRA_LIB_SYSTEMD_DAEMON_OPTIONAL

dnl Check for the CMU GPUT library.
RRA_LIB_GPUT

dnl Check for regex libraries for pcre:* and regex:* ACL support.
RRA_LIB_PCRE2_OPTIONAL
AS_IF([test x"$rra_use_PCRE2" != xtrue], [RRA_LIB_PCRE_OPTIONAL])
AC_CHECK_HEADER([regex.h], [AC_CHECK_FUNCS([regcomp])])

dnl General C library and networking probes.
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([sys/bitypes.h sys/filio.h sys/select.h sys/time.h \
                  sys/uio.h syslog.h])
AC_CHECK_DECLS([reallocarray])
AC_CHECK_DECLS([h_errno], [], [], [#include <netdb.h>])
AC_CHECK_DECLS([inet_aton, inet_ntoa], [], [],
    [#include <sys/types.h>
     #include <netinet/in.h>
     #include <arpa/inet.h>])
RRA_C_C99_VAMACROS
RRA_C_GNU_VAMACROS
AC_CHECK_MEMBERS([struct sockaddr.sa_len], [], [],
    [#include <sys/types.h>
     #include <sys/socket.h>])
AC_TYPE_LONG_LONG_INT
AC_TYPE_UINT32_T
AC_CHECK_TYPES([sig_atomic_t], [], [],
    [#include <sys/types.h>
     #include <signal.h>])
AC_CHECK_TYPES([ssize_t], [], [],
    [#include <sys/types.h>])
AC_CHECK_TYPES([socklen_t], [], [],
    [#include <sys/types.h>
     #include <sys/socket.h>])
AC_CHECK_TYPES([struct sockaddr_in6],
    [AC_DEFINE([HAVE_INET6], [1],
        [Define to 1 if IPv6 library interfaces are available.])], [],
    [#include <sys/types.h>
     #include <netinet/in.h>])
AC_CHECK_TYPES([struct sockaddr_storage],
    [AC_CHECK_MEMBERS([struct sockaddr_storage.ss_family], [], [],
        [#include <sys/types.h>
         #include <sys/socket.h>])], [],
    [#include <sys/types.h>
     #include <sys/socket.h>])
RRA_MACRO_IN6_ARE_ADDR_EQUAL
RRA_FUNC_INET_NTOA
AC_CHECK_FUNCS([getaddrinfo],
    [RRA_FUNC_GETADDRINFO_ADDRCONFIG],
    [AC_LIBOBJ([getaddrinfo])])
AC_CHECK_FUNCS([getgrnam_r setrlimit setsid])
AC_REPLACE_FUNCS([asprintf daemon getnameinfo getopt inet_aton inet_ntop \
                  mkstemp reallocarray setenv strndup])

dnl Whether to build the Perl bindings.  Put this late so that it shows up
dnl near the bottom of the --help output.
build_perl=
AC_ARG_ENABLE([perl],
    [AS_HELP_STRING([--enable-perl], [Build Perl libremctl bindings])],
    [AS_IF([test x"$enableval" = xyes], [build_perl=yes])])
AM_CONDITIONAL([BUILD_PERL], [test x"$build_perl" = xyes])
AS_IF([test x"$build_perl" = xyes],
    [RRA_PROG_PERL([5.008])
     PERL_LDFLAGS=`"$PERL" -MConfig -e 'print $Config{lddlflags}'`])
AC_SUBST([PERL_LDFLAGS])

dnl Whether to build the PHP bindings.  The dance required to be able to run
dnl phpize before AC_OUTPUT sucks like a sucking thing.
build_php=
AC_ARG_ENABLE([php],
    [AS_HELP_STRING([--enable-php], [Build remctl PECL extension for PHP])],
    [AS_IF([test x"$enableval" = xyes],
        [build_php=yes
         AC_ARG_VAR([PHPIZE], [Path to phpize])
         AC_PATH_PROG([PHPIZE], [phpize])
         AS_IF([test x"$PHPIZE" = x],
             [AC_MSG_ERROR([cannot build PECL extension without phpize])])])])
AM_CONDITIONAL([BUILD_PHP], [test x"$build_php" = xyes])

dnl Whether to build the Python bindings.
build_python=
AC_ARG_ENABLE([python],
    [AS_HELP_STRING([--enable-python], [Build Python libremctl bindings])],
    [AS_IF([test x"$enableval" = xyes], [build_python=yes])])
AM_CONDITIONAL([BUILD_PYTHON], [test x"$build_python" = xyes])
AS_IF([test x"$build_python" = xyes],
    [RRA_PROG_PYTHON([2.3], [3.1])])

dnl Whether to build the Ruby bindings.
build_ruby=
AC_ARG_ENABLE([ruby],
    [AS_HELP_STRING([--enable-ruby], [Build Ruby libremctl bindings])],
    [AS_IF([test x"$enableval" = xyes], [build_ruby=yes])])
AM_CONDITIONAL([BUILD_RUBY], [test x"$build_ruby" = xyes])

dnl Used when building the Perl module.  Normally link against all the
dnl dependencies that libremctl may need, but don't do so if
dnl --enable-reduced-depends was specified.
AS_IF([test x"$rra_reduced_depends" = xtrue],
    [DEPEND_LIBS=],
    [DEPEND_LIBS="$GSSAPI_LDFLAGS $GSSAPI_LIBS $KRB5_LDFLAGS $KRB5_LIBS"])
AC_SUBST([DEPEND_LIBS])

AC_CONFIG_FILES([Makefile java/build.xml java/local.properties])
AC_CONFIG_FILES([tests/data/conf-simple])
AS_IF([test x"$build_php" = xyes],
    [AC_CONFIG_FILES([php/config.m4 php/php_remctl.h])])
AS_IF([test x"$build_ruby" = xyes],
    [AC_CONFIG_FILES([ruby/extconf.rb ruby/test_remctl.rb])])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_COMMANDS([docs], [test -d docs || mkdir docs])
AC_CONFIG_COMMANDS([tests/config],
    [test -d tests/config || mkdir tests/config])
AC_OUTPUT

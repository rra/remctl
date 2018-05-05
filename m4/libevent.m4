dnl Find the compiler and linker flags for libevent.
dnl
dnl Finds the compiler and linker flags for linking with the libevent library.
dnl Provides the --with-libevent, --with-libevent-lib, and
dnl --with-libevent-include configure options to specify non-standard paths to
dnl the libevent libraries or header files.
dnl
dnl Provides the macros RRA_LIB_EVENT and RRA_LIB_EVENT_OPTIONAL and sets the
dnl substitution variables LIBEVENT_CPPFLAGS, LIBEVENT_LDFLAGS, and
dnl LIBEVENT_LIBS.  Also provides RRA_LIB_EVENT_SWITCH to set CPPFLAGS,
dnl LDFLAGS, and LIBS to include the libevent libraries, saving the current
dnl values first, and RRA_LIB_EVENT_RESTORE to restore those settings to
dnl before the last RRA_LIB_EVENT_SWITCH.  Defines HAVE_LIBEVENT and sets
dnl rra_use_LIBEVENT to true if libevent is found.  If it isn't found, the
dnl substitution variables will be empty.
dnl
dnl Also provides RRA_INCLUDES_EVENT, which are the headers to include when
dnl probing the libevent library properties.  This assumes that
dnl AC_CHECK_HEADERS([event2/event.h]) has been called.
dnl
dnl Depends on the lib-helper.m4 framework.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2014
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Headers to include when probing for libevent declarations.
AC_DEFUN([RRA_INCLUDES_EVENT], [[
#if HAVE_EVENT2_EVENT_H
# include <event2/buffer.h>
# include <event2/bufferevent.h>
# include <event2/event.h>
#else
# include <event.h>
#endif
]])

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the libevent flags.  Used as a wrapper, with
dnl RRA_LIB_LIBEVENT_RESTORE, around tests.
AC_DEFUN([RRA_LIB_EVENT_SWITCH], [RRA_LIB_HELPER_SWITCH([LIBEVENT])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values before
dnl RRA_LIB_LIBEVENT_SWITCH was called.
AC_DEFUN([RRA_LIB_EVENT_RESTORE], [RRA_LIB_HELPER_RESTORE([LIBEVENT])])

dnl Checks if libevent is present.  The single argument, if "true", says to
dnl fail if the libevent library could not be found.  Prefer probing with
dnl pkg-config if available and the --with flags were not given.  Failing
dnl that, prefer linking with libevent_core over libevent.  (This means that
dnl callers will need to add the non-core libraries if desired.)
AC_DEFUN([_RRA_LIB_EVENT_INTERNAL],
[AC_REQUIRE([RRA_ENABLE_REDUCED_DEPENDS])
 RRA_LIB_HELPER_PATHS([LIBEVENT])
 AS_IF([test x"$LIBEVENT_CPPFLAGS" = x && test x"$LIBEVENT_LDFLAGS" = x],
    [PKG_CHECK_EXISTS([libevent],
        [PKG_CHECK_MODULES([LIBEVENT], [libevent])
         LIBEVENT_CPPFLAGS="$LIBEVENT_CFLAGS"])])
 AS_IF([test x"$LIBEVENT_LIBS" = x],
    [RRA_LIB_EVENT_SWITCH
     LIBS=
     AC_SEARCH_LIBS([event_base_free], [event_core event],
        [LIBEVENT_LIBS="$LIBS"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable libevent library])])])
     RRA_LIB_EVENT_RESTORE])])

dnl The main macro for packages with mandatory libevent support.
AC_DEFUN([RRA_LIB_EVENT],
[RRA_LIB_HELPER_VAR_INIT([LIBEVENT])
 RRA_LIB_HELPER_WITH([libevent], [libevent], [LIBEVENT])
 _RRA_LIB_EVENT_INTERNAL([true])
 rra_use_LIBEVENT=true
 AC_DEFINE([HAVE_LIBEVENT], 1, [Define if libevent is available.])])

dnl The main macro for packages with optional libevent support.
AC_DEFUN([RRA_LIB_EVENT_OPTIONAL],
[RRA_LIB_HELPER_VAR_INIT([LIBEVENT])
 RRA_LIB_HELPER_WITH_OPTIONAL([libevent], [libevent], [LIBEVENT])
 AS_IF([test x"$rra_use_LIBEVENT" != xfalse],
    [AS_IF([test x"$rra_use_LIBEVENT" = xtrue],
        [_RRA_LIB_EVENT_INTERNAL([true])],
        [_RRA_LIB_EVENT_INTERNAL([false])])])
 AS_IF([test x"$LIBEVENT_LIBS" != x],
    [rra_use_LIBEVENT=true
     AC_DEFINE([HAVE_LIBEVENT], 1, [Define if libevent is available.])])])

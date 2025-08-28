# serial 1

dnl Find the compiler and linker flags for PCRE2.
dnl
dnl Finds the compiler and linker flags for linking with the PCRE2 library.
dnl Provides the --with-pcre2, --with-pcre2-lib, and --with-pcre2-include
dnl configure options to specify non-standard paths to the PCRE libraries.
dnl Uses pcre-config where available.  Only the 8-bit PCRE2 library is
dnl currently supported.
dnl
dnl Provides the macro RRA_LIB_PCRE2 and sets the substitution
dnl variables PCRE2_CPPFLAGS, PCRE2_LDFLAGS, and PCRE2_LIBS.  Also provides
dnl RRA_LIB_PCRE2_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the PCRE2
dnl libraries, saving the current values first, and RRA_LIB_PCRE2_RESTORE to
dnl restore those settings to before the last RRA_LIB_PCRE2_SWITCH.  Defines
dnl HAVE_PCRE2 and sets rra_use_PCRE2.
dnl
dnl Provides the RRA_LIB_PCRE2_OPTIONAL macro, which should be used if PCRE2
dnl support is optional.  This macro will still always set the substitution
dnl variables, but they'll be empty if the PCRE2 library is not found or if
dnl --without-pcre2 is given.  Defines HAVE_PCRE2 and sets rra_use_PCRE2 to
dnl true if the PCRE2 library is found and --without-pcre2 is not given.
dnl
dnl Depends on the lib-helper.m4 framework and the Autoconf macros that come
dnl with pkg-config.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Written by Russ Allbery <eagle@eyrie.org>
dnl Copyright 2021-2022 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2010, 2013
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.
dnl
dnl SPDX-License-Identifier: FSFULLR

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the PCRE2 flags.  Used as a wrapper, with
dnl RRA_LIB_PCRE2_RESTORE, around tests.
AC_DEFUN([RRA_LIB_PCRE2_SWITCH], [RRA_LIB_HELPER_SWITCH([PCRE2])])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values before
dnl RRA_LIB_PCRE2_SWITCH was called.
AC_DEFUN([RRA_LIB_PCRE2_RESTORE], [RRA_LIB_HELPER_RESTORE([PCRE2])])

dnl Extra preprocessor directives for probing the pcre2.h header.
AC_DEFUN([_RRA_INCLUDES_PCRE2], [[
#define PCRE2_CODE_UNIT_WIDTH 8
]])

dnl Checks if PCRE2 is present.  The single argument, if "true", says to fail
dnl if the PCRE2 library could not be found.  Prefer probing with pkg-config
dnl if available and the --with flags were not given.
AC_DEFUN([_RRA_LIB_PCRE2_INTERNAL],
[RRA_LIB_HELPER_PATHS([PCRE2])
 AS_IF([test x"$PCRE2_CPPFLAGS" = x && test x"$PCRE2_LDFLAGS" = x],
    [PKG_CHECK_EXISTS([libpcre2-8],
        [PKG_CHECK_MODULES([PCRE2], [libpcre2-8])
         PCRE2_CPPFLAGS="$PCRE2_CFLAGS"])])
 AS_IF([test x"$PCRE2_LIBS" = x],
    [RRA_LIB_PCRE2_SWITCH
     LIBS=
     AC_SEARCH_LIBS([pcre2_compile_8], [pcre2-8],
        [PCRE2_LIBS="$LIBS"],
        [AS_IF([test x"$1" = xtrue],
            [AC_MSG_ERROR([cannot find usable PCRE2 library])])])
     RRA_LIB_PCRE2_RESTORE])
 RRA_LIB_PCRE2_SWITCH
 AC_CHECK_HEADERS([pcre2.h], [],
    [AS_IF([test x"$1" = xtrue],
        [AC_MSG_ERROR([cannot find usable PCRE2 header])])],
    [_RRA_INCLUDES_PCRE2])])

dnl The main macro for packages with mandatory PCRE2 support.
AC_DEFUN([RRA_LIB_PCRE2],
[RRA_LIB_HELPER_VAR_INIT([PCRE2])
 RRA_LIB_HELPER_WITH([pcre2], [PCRE2], [PCRE2])
 _RRA_LIB_PCRE2_INTERNAL([true])
 rra_use_PCRE2=true
 AC_DEFINE([HAVE_PCRE2], 1, [Define if PCRE2 is available.])])

dnl The main macro for packages with optional PCRE2 support.
AC_DEFUN([RRA_LIB_PCRE2_OPTIONAL],
[RRA_LIB_HELPER_VAR_INIT([PCRE2])
 RRA_LIB_HELPER_WITH_OPTIONAL([pcre2], [PCRE2], [PCRE2])
 AS_IF([test x"$rra_use_PCRE2" != xfalse],
    [AS_IF([test x"$rra_use_PCRE2" = xtrue],
        [_RRA_LIB_PCRE2_INTERNAL([true])],
        [_RRA_LIB_PCRE2_INTERNAL([false])])])
 AS_IF([test x"$PCRE2_LIBS" = x],
    [RRA_LIB_HELPER_VAR_CLEAR([PCRE2])],
    [rra_use_PCRE2=true
     AC_DEFINE([HAVE_PCRE2], 1, [Define if PCRE2 is available.])])])

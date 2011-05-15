dnl Find the compiler and linker flags for PCRE.
dnl
dnl Finds the compiler and linker flags for linking with the PCRE library.
dnl Provides the --with-pcre, --with-pcre-lib, and --with-pcre-include
dnl configure options to specify non-standard paths to the Kerberos libraries.
dnl Uses pcre-config where available.
dnl
dnl Provides the macro RRA_LIB_PCRE_OPTIONAL and sets the substitution
dnl variables PCRE_CPPFLAGS, PCRE_LDFLAGS, and PCRE_LIBS.  Also provides
dnl RRA_LIB_PCRE_SWITCH to set CPPFLAGS, LDFLAGS, and LIBS to include the PCRE
dnl libraries, saving the current values first, and RRA_LIB_PCRE_RESTORE to
dnl restore those settings to before the last RRA_LIB_PCRE_SWITCH.  HAVE_PCRE
dnl will be defined if PCRE is found.  If it isn't found, the substitution
dnl variables will be empty.
dnl
dnl Depends on RRA_SET_LDFLAGS.
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2010
dnl     The Board of Trustees of the Leland Stanford Junior University
dnl
dnl This file is free software; the authors give unlimited permission to copy
dnl and/or distribute it, with or without modifications, as long as this
dnl notice is preserved.

dnl Save the current CPPFLAGS, LDFLAGS, and LIBS settings and switch to
dnl versions that include the PCRE flags.  Used as a wrapper, with
dnl RRA_LIB_PCRE_RESTORE, around tests.
AC_DEFUN([RRA_LIB_PCRE_SWITCH],
[rra_pcre_save_CPPFLAGS="$CPPFLAGS"
 rra_pcre_save_LDFLAGS="$LDFLAGS"
 rra_pcre_save_LIBS="$LIBS"
 CPPFLAGS="$PCRE_CPPFLAGS $CPPFLAGS"
 LDFLAGS="$PCRE_LDFLAGS $LDFLAGS"
 LIBS="$PCRE_LIBS $LIBS"])

dnl Restore CPPFLAGS, LDFLAGS, and LIBS to their previous values (before
dnl RRA_LIB_PCRE_SWITCH was called).
AC_DEFUN([RRA_LIB_PCRE_RESTORE],
[CPPFLAGS="$rra_pcre_save_CPPFLAGS"
 LDFLAGS="$rra_pcre_save_LDFLAGS"
 LIBS="$rra_pcre_save_LIBS"])

dnl Set PCRE_CPPFLAGS and PCRE_LDFLAGS based on rra_pcre_root,
dnl rra_pcre_libdir, and rra_pcre_includedir.
AC_DEFUN([_RRA_LIB_PCRE_PATHS],
[AS_IF([test x"$rra_pcre_libdir" != x],
    [PCRE_LDFLAGS="-L$rra_pcre_libdir"],
    [AS_IF([test x"$rra_pcre_root" != x],
        [RRA_SET_LDFLAGS([PCRE_LDFLAGS], [$rra_pcre_root])])])
 AS_IF([test x"$rra_pcre_includedir" != x],
    [PCRE_CPPFLAGS="-I$rra_pcre_includedir"],
    [AS_IF([test x"$rra_pcre_root" != x],
        [AS_IF([test x"$rra_pcre_root" != x/usr],
            [PCRE_CPPFLAGS="-I${rra_pcre_root}/include"])])])])

dnl Does the appropriate library checks for PCRE linkage without pcre-config.
dnl The single argument, if true, says to fail if PCRE could not be found.
AC_DEFUN([_RRA_LIB_PCRE_MANUAL],
[RRA_LIB_PCRE_SWITCH
 AC_CHECK_LIB([pcre], [pcre_compile], [PCRE_LIBS="-lpcre"],
     [AS_IF([test x"$1" = xtrue],
         [AC_MSG_ERROR([cannot find usable PCRE library])])])
 RRA_LIB_PCRE_RESTORE])

dnl Sanity-check the results of pcre-config and be sure we can really link a
dnl PCRE program.  If that fails, clear PCRE_CPPFLAGS and PCRE_LIBS so that we
dnl know we don't have usable flags and fall back on the manual check.
AC_DEFUN([_RRA_LIB_PCRE_CHECK],
[RRA_LIB_PCRE_SWITCH
 AC_CHECK_FUNC([pcre_compile],
    [RRA_LIB_PCRE_RESTORE],
    [RRA_LIB_PCRE_RESTORE
     PCRE_CPPFLAGS=
     PCRE_LIBS=
     _RRA_LIB_PCRE_PATHS
     _RRA_LIB_PCRE_MANUAL([$1])])])

dnl The core of the PCRE library checking.  The single argument, if "true",
dnl says to fail if PCRE could not be found.
AC_DEFUN([_RRA_LIB_PCRE_INTERNAL],
[AC_REQUIRE([RRA_ENABLE_REDUCED_DEPENDS])
 AC_ARG_VAR([PCRE_CONFIG], [Path to pcre-config])
 AS_IF([test x"$rra_pcre_root" != x && test -z "$PCRE_CONFIG"],
    [AS_IF([test -x "${rra_pcre_root}/bin/pcre-config"],
        [PCRE_CONFIG="${rra_pcre_root}/bin/pcre-config"])],
    [AC_PATH_PROG([PCRE_CONFIG], [pcre-config])])
 AS_IF([test x"$PCRE_CONFIG" != x && test -x "$PCRE_CONFIG"],
    [PCRE_CPPFLAGS=`"$PCRE_CONFIG" --cflags 2>/dev/null`
     PCRE_LIBS=`"$PCRE_CONFIG" --libs 2>/dev/null`
     PCRE_CPPFLAGS=`echo "$PCRE_CPPFLAGS" | sed 's%-I/usr/include ?%%'`
     _RRA_LIB_PCRE_CHECK([$1])],
    [_RRA_LIB_PCRE_PATHS
     _RRA_LIB_PCRE_MANUAL([$1])])])])

dnl The main macro for packages with optional PCRE support.
AC_DEFUN([RRA_LIB_PCRE_OPTIONAL],
[rra_pcre_root=
 rra_pcre_libdir=
 rra_pcre_includedir=
 rra_with_pcre=
 PCRE_CPPFLAGS=
 PCRE_LDFLAGS=
 PCRE_LIBS=
 AC_SUBST([PCRE_CPPFLAGS])
 AC_SUBST([PCRE_LDFLAGS])
 AC_SUBST([PCRE_LIBS])

 AC_ARG_WITH([pcre],
    [AS_HELP_STRING([--with-pcre@<:@=DIR@:>@],
        [Location of PCRE headers and libraries])],
    [AS_IF([test x"$withval" = xno],
        [rra_use_pcre=false],
        [AS_IF([test x"$withval" != xyes], [rra_pcre_root="$withval"])
         rra_use_pcre=true])])
 AC_ARG_WITH([pcre-include],
    [AS_HELP_STRING([--with-pcre-include=DIR],
        [Location of PCRE headers])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_pcre_includedir="$withval"])])
 AC_ARG_WITH([pcre-lib],
    [AS_HELP_STRING([--with-pcre-lib=DIR],
        [Location of PCRE libraries])],
    [AS_IF([test x"$withval" != xyes && test x"$withval" != xno],
        [rra_pcre_libdir="$withval"])])

 AS_IF([test x"$rra_use_pcre" != xfalse],
     [AS_IF([test x"$rra_use_pcre" = xtrue],
         [_RRA_LIB_PCRE_INTERNAL([true])],
         [_RRA_LIB_PCRE_INTERNAL([false])])])
 AS_IF([test x"$PCRE_LIBS" != x],
    [AC_DEFINE([HAVE_PCRE], 1,
        [Define to 1 if the PCRE library is present.])])])

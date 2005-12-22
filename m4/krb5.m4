dnl krb5.m4 -- Find the compiler and linker flags for Kerberos v5.
dnl $Id$
dnl
dnl Finds the compiler and linker flags and adds them to CPPFLAGS and LIBS.
dnl Provides --with-kerberos, --enable-reduced-depends, and --enable-static
dnl configure options to control how linking with Kerberos is done.  Uses
dnl krb5-config where available unless reduced dependencies is requested.
dnl
dnl Provides the macro RRA_LIB_KRB5, which takes two arguments.  The first
dnl argument is the type of Kerberos libraries desired (one of the arguments
dnl to krb5-config).  The second argument is whether to probe for networking
dnl libraries in the non-krb5-config, non-reduced-dependencies case and should
dnl be either "true" (if the program doesn't otherwise use the networking
dnl libraries) or "false" (if it is already probing for the networking
dnl libraries separately).

AC_DEFUN([RRA_LIB_KRB5],
[KRBROOT=
AC_ARG_WITH([kerberos],
    AC_HELP_STRING([--with-kerberos=DIR],
        [Location of Kerberos headers and libraries]),
    [if test x"$withval" != xno ; then
        KRBROOT="$withval"
     fi])

reduce_depends=false
AC_ARG_ENABLE([reduced-depends],
    AC_HELP_STRING([--enable-reduced-depends],
        [Try to minimize shared library dependencies]),
    [if test x"$enableval" = xyes ; then
         if test x"$KRBROOT" != x ; then
             if test x"$KRBROOT" != x/usr ; then
                 CPPFLAGS="-I$KRBROOT/include"
             fi
             LDFLAGS="$LDFLAGS -L$KRBROOT/lib"
         fi
         AC_CHECK_LIB([gssapi], [gss_import_name],
             [KRBLIBS="-lgssapi"],
             [AC_CHECK_LIB([gssapi_krb5], [gss_import_name],
                 [KRBLIBS="-lgssapi_krb5"],
                 [AC_MSG_ERROR([cannot find usable GSSAPI library])])])
         reduce_depends=true
     fi])

dnl Checking for the neworking libraries shouldn't be necessary for the
dnl krb5-config case, but apparently it is at least for MIT Kerberos 1.2.
dnl This will unfortunately mean multiple -lsocket -lnsl references when
dnl building with current versions of Kerberos, but this shouldn't cause
dnl any practical problems.
if test x"$reduce_depends" != xtrue ; then
    if test x"$2" = xtrue ; then
        AC_SEARCH_LIBS([gethostbyname], [nsl])
        AC_SEARCH_LIBS([socket], [socket], ,
            [AC_CHECK_LIB([nsl], [socket],
                [LIBS="-lnsl -lsocket $LIBS"], , [-lsocket])])
    fi
    AC_ARG_VAR([KRB5_CONFIG], [Path to krb5-config])
    if test x"$KRBROOT" != x ; then
        if test -x "$KRBROOT/bin/krb5-config" ; then
            KRB5_CONFIG="$KRBROOT/bin/krb5-config"
        fi
    else
        AC_PATH_PROG([KRB5_CONFIG], [krb5-config])
    fi
    if test x"$KRB5_CONFIG" != x ; then
        AC_MSG_CHECKING([for $1 support in krb5-config])
        if "$KRB5_CONFIG" | grep '$1' > /dev/null 2>&1 ; then
            AC_MSG_RESULT([yes])
            KRBCPPFLAGS=`"$KRB5_CONFIG" --cflags '$1'`
            KRBLIBS=`"$KRB5_CONFIG" --libs '$1'`
        else
            AC_MSG_RESULT([no])
            KRBCPPFLAGS=`"$KRB5_CONFIG" --cflags`
            KRBLIBS=`"$KRB5_CONFIG" --libs`
        fi
        KRBCPPFLAGS=`echo "$KRBCPPFLAGS" | sed 's%-I/usr/include ?%%'`
    else
        if test x"$KRBROOT" != x ; then
            if test x"$KRBROOT" != x/usr ; then
                KRBCPPFLAGS="-I$KRBROOT/include"
            fi
            KRBLIBS="-L$KRBROOT/lib"
        fi
        AC_SEARCH_LIBS([res_search], [resolv])
        AC_SEARCH_LIBS([crypt], [crypt])
        AC_CHECK_LIB([gssapi], [gss_import_name],
            [KRBLIBS="-lgssapi -lkrb5 -lasn1 -lroken -lcrypto -lcom_err"],
            [KRB5EXTRA="-lkrb5 -lk5crypto -lcom_err"
             AC_CHECK_LIB([krb5support], [main],
                [KRB5EXTRA="$KRB5EXTRA -lkrb5support"])
             AC_CHECK_LIB([gssapi_krb5], [gss_import_name],
                [KRBLIBS="-lgssapi_krb5 $KRB5EXTRA"],
                [AC_MSG_ERROR([cannot find usable GSSAPI library])],
                [$KRB5EXTRA])],
            [-lkrb5 -lasn1 -lroken -lcrypto -lcom_err])
    fi
    if test x"$KRBCPPFLAGS" != x ; then
        CPPFLAGS="$CPPFLAGS $KRBCPPFLAGS"
    fi
fi

AC_ARG_ENABLE([static],
    AC_HELP_STRING([--enable-static],
        [Link against the static Kerberos libraries]),
    [if test x"$enableval" = xyes ; then
         LIBS="-Wl,-Bstatic $KRBLIBS -Wl,-Bdynamic $LIBS"
     else
         LIBS="$KRBLIBS $LIBS"
     fi],
    [LIBS="$KRBLIBS $LIBS"])
CPPFLAGS=`echo "$CPPFLAGS" | sed 's/^  *//'`
LDFLAGS=`echo "$LDFLAGS" | sed 's/^  *//'`

dnl Check a few more things for portability between MIT and Heimdal if GSSAPI
dnl support was requested.
AC_CHECK_HEADERS([gssapi.h])
AC_CHECK_DECL([GSS_C_NT_USER_NAME],
    [AC_DEFINE([HAVE_GSS_RFC_OIDS], 1,
       [Define to 1 if the GSS-API library uses RFC-compliant OIDs.])], ,
[#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi.h>
#endif
])
AC_CHECK_DECLS([GSS_KRB5_MECHANISM], , ,
[#ifdef HAVE_GSSAPI_H
# include <gssapi.h>
#else
# include <gssapi/gssapi.h>
#endif
])])

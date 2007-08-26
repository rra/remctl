dnl krb5.m4 -- Find the compiler and linker flags for Kerberos v5.
dnl $Id$
dnl
dnl Finds the compiler and linker flags and adds them to CPPFLAGS and LIBS.
dnl Provides --with-kerberos and --enable-reduced-depends configure options
dnl to control how linking with Kerberos is done.  Uses krb5-config where
dnl available unless reduced dependencies is requested.
dnl
dnl Provides the macro RRA_LIB_KRB5, which takes two arguments.  The first
dnl argument is the type of Kerberos libraries desired (one of the arguments
dnl to krb5-config).  The second argument is whether to probe for networking
dnl libraries in the non-krb5-config, non-reduced-dependencies case and should
dnl be either "true" (if the program doesn't otherwise use the networking
dnl libraries) or "false" (if it is already probing for the networking
dnl libraries separately).
dnl
dnl Written by Russ Allbery <rra@stanford.edu>
dnl Copyright 2005, 2006 Board of Trustees, Leland Stanford Jr. University
dnl See README for licensing terms.

dnl Does the appropriate library checks for reduced-dependency GSS-API
dnl linkage.  Check for MIT Kerberos first since it has the least generic name
dnl for the GSS-API library.
AC_DEFUN([_RRA_LIB_KRB5_GSSAPI_REDUCED],
[AC_CHECK_LIB([gssapi_krb5], [gss_import_name],
    [KRBLIBS="-lgssapi_krb5"],
    [AC_CHECK_LIB([gssapi], [gss_import_name],
        [KRBLIBS="-lgssapi"],
        [AC_MSG_ERROR([cannot find usable GSS-API library])])])])

dnl Does the appropriate library checks for reduced-dependency krb5 linkage.
AC_DEFUN([_RRA_LIB_KRB5_KRB5_REDUCED],
[AC_CHECK_LIB([krb5], [krb5_init_context], [KRBLIBS="-lkrb5"],
    [AC_MSG_ERROR([cannot find usable Kerberos v5 library])])
AC_CHECK_LIB([com_err], [com_err], [KRBLIBS="$KRBLIBS -lcom_err"],
    [AC_MSG_ERROR([cannot find usable com_err library])])])

dnl Does the appropriate library checks for reduced-dependency krb4 linkage.
AC_DEFUN([_RRA_LIB_KRB5_KRB4_REDUCED],
[AC_CHECK_LIB([krb4], [krb_get_svc_in_tkt], [KRBLIBS="-lkrb4"],
    [AC_CHECK_LIB([krb], [krb_get_svc_in_tkt], [KRBLIBS="-lkrb"],
        [AC_MSG_ERROR([cannot find usable Kerberos v4 library])])])])

dnl Does the appropriate library checks for GSS-API linkage.
AC_DEFUN([_RRA_LIB_KRB5_GSSAPI],
[AC_CHECK_LIB([gssapi], [gss_import_name],
    [KRBLIBS="-lgssapi -lkrb5 -lasn1 -lroken -lcrypto -lcom_err"],
    [KRB5EXTRA="-lkrb5 -lk5crypto -lcom_err"
     AC_CHECK_LIB([krb5support], [krb5int_getspecific],
        [KRB5EXTRA="$KRB5EXTRA -lkrb5support"],
        [AC_SEARCH_LIBS([pthread_setspecific], [pthreads pthread])
         AC_CHECK_LIB([krb5support], [krb5int_setspecific],
            [KRB5EXTRA="$KRB5EXTRA -lkrb5support"])])
     AC_CHECK_LIB([gssapi_krb5], [gss_import_name],
        [KRBLIBS="-lgssapi_krb5 $KRB5EXTRA"],
        [AC_MSG_ERROR([cannot find usable GSS-API library])],
        [$KRB5EXTRA])],
    [-lkrb5 -lasn1 -lroken -lcrypto -lcom_err])])

dnl Does the appropriate library checks for krb5 linkage.  Note that we have
dnl to check for a different function the second time since the Heimdal and
dnl MIT libraries have the same name.
AC_DEFUN([_RRA_LIB_KRB5_KRB5],
[AC_CHECK_LIB([krb5], [krb5_init_context],
    [KRBLIBS="-lkrb5 -lasn1 -lroken -lcrypto -lcom_err"],
    [KRB5EXTRA="-lk5crypto -lcom_err"
     AC_CHECK_LIB([krb5support], [krb5int_getspecific],
        [KRB5EXTRA="$KRB5EXTRA -lkrb5support"],
        [AC_SEARCH_LIBS([pthread_setspecific], [pthreads pthread])
         AC_CHECK_LIB([krb5support], [krb5int_setspecific],
            [KRB5EXTRA="$KRB5EXTRA -lkrb5support"])])
     AC_CHECK_LIB([krb5], [krb5_cc_default],
        [KRBLIBS="-lkrb5 $KRB5EXTRA"],
        [AC_MSG_ERROR([cannot find usable Kerberos v5 library])],
        [$KRB5EXTRA])],
    [-lasn1 -lroken -lcrypto -lcom_err])])

dnl Does the appropriate library checks for krb4 linkage.
AC_DEFUN([_RRA_LIB_KRB5_KRB4],
[KRB4EXTRA=
AC_CHECK_LIB([crypto], [des_set_key], [KRB4EXTRA="-lcrypto"],
    [KRB4EXTRA="-ldes"])
AC_CHECK_LIB([krb], [krb_get_svc_in_tkt],
    [KRBLIBS="-lkrb $KRB4EXTRA"],
    [KRB5EXTRA="-ldes425 -lkrb5 -lk5crypto -lcom_err"
     AC_CHECK_LIB([krb5support], [krb5int_getspecific],
        [KRB5EXTRA="$KRB5EXTRA -lkrb5support"],
        [AC_SEARCH_LIBS([pthread_setspecific], [pthreads pthread])
         AC_CHECK_LIB([krb5support], [krb5int_setspecific],
            [KRB5EXTRA="$KRB5EXTRA -lkrb5support"])])
     AC_CHECK_LIB([krb4], [krb_get_svc_in_tkt],
        [KRBLIBS="-lkrb4 $KRB5EXTRA"],
        [AC_MSG_ERROR([cannot find usable Kerberos v4 library])],
        [$KRB5EXTRA])],
    [$KRB4EXTRA])])

dnl Additional checks for portability between MIT and Heimdal if GSS-API
dnl libraries were requested.
AC_DEFUN([_RRA_LIB_KRB5_GSSAPI_EXTRA],
[AC_CHECK_HEADERS([gssapi.h])
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

dnl Additional checks for portability between MIT and Heimdal if krb5
dnl libraries were requested.
AC_DEFUN([_RRA_LIB_KRB5_KRB5_EXTRA],
[AC_CHECK_HEADERS([et/com_err.h])
AC_CHECK_FUNCS([krb5_free_keytab_entry_contents])
AC_CHECK_TYPES([krb5_realm], , , [#include <krb5.h>])])

dnl Additional checks for portability if krb4 libraries were requested.
AC_DEFUN([_RRA_LIB_KRB5_KRB4_EXTRA],
[AC_CHECK_HEADERS([kerberosIV/krb.h])])

dnl The main macro.
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
         case "$1" in
         gssapi) _RRA_LIB_KRB5_GSSAPI_REDUCED ;;
         krb5)   _RRA_LIB_KRB5_KRB5_REDUCED   ;;
         krb4)   _RRA_LIB_KRB5_KRB4_REDUCED   ;;
         *)      AC_MSG_ERROR([BUG: unknown library type $1]) ;;
         esac
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
            LDFLAGS="$LDFLAGS -L$KRBROOT/lib"
        fi
        AC_SEARCH_LIBS([res_search], [resolv], ,
            [AC_SEARCH_LIBS([__res_search], [resolv])])
        AC_SEARCH_LIBS([crypt], [crypt])
        case "$1" in
        gssapi) _RRA_LIB_KRB5_GSSAPI ;;
        krb5)   _RRA_LIB_KRB5_KRB5   ;;
        krb4)   _RRA_LIB_KRB5_KRB4   ;;
        *)      AC_MSG_ERROR([BUG: unknown library type $1]) ;;
        esac
    fi
    if test x"$KRBCPPFLAGS" != x ; then
        CPPFLAGS="$CPPFLAGS $KRBCPPFLAGS"
    fi
fi

dnl Generate the final library list and put it into the standard variables.
LIBS="$KRBLIBS $LIBS"
CPPFLAGS=`echo "$CPPFLAGS" | sed 's/^  *//'`
LDFLAGS=`echo "$LDFLAGS" | sed 's/^  *//'`

dnl Run any extra checks for the desired libraries.
case "$1" in
gssapi) _RRA_LIB_KRB5_GSSAPI_EXTRA ;;
krb5)   _RRA_LIB_KRB5_KRB5_EXTRA   ;;
krb4)   _RRA_LIB_KRB5_KRB4_EXTRA   ;;
esac])

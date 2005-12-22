dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_REVISION($Revision$)
AC_PREREQ(2.57)
AC_INIT([remctl], [1.10], [rra@stanford.edu])

m4_include([m4/snprintf.m4])

AC_PROG_CC
AC_PROG_INSTALL

AC_ARG_WITH([kerberos],
    AC_HELP_STRING([--with-kerberos=DIR],
        [Location of Kerberos headers and libraries]),
    [if test x"$withval" != xno ; then
        KRBLIBS="-L$withval/lib"
        CPPFLAGS="$CPPFLAGS -I$withval/include"
     fi])

reduce_depends=false
AC_ARG_ENABLE([reduced-depends],
    AC_HELP_STRING([--enable-reduced-depends],
        [Try to minimize shared library dependencies]),
    [if test x"$enableval" = xyes ; then
         KRBLIBS="$KRBLIBS -lgssapi_krb5"
         reduce_depends=true
     else
         KRBLIBS="$KRBLIBS -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err"
     fi],
    [KRBLIBS="$KRBLIBS -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err"])

AC_SEARCH_LIBS(gethostbyname, nsl)
AC_SEARCH_LIBS(socket, socket, ,
    [AC_CHECK_LIB(nsl, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)])

if test x"$reduce_depends" != xtrue ; then
    AC_SEARCH_LIBS(res_search, resolv)
    AC_SEARCH_LIBS(crypt, crypt)
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

AC_CHECK_TYPES([long long])
RRA_FUNC_SNPRINTF
AC_TYPE_SIGNAL

AC_CONFIG_HEADER(config.h)
AC_OUTPUT(Makefile)

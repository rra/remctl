dnl Process this file with autoconf to produce a configure script.
dnl $Id$

AC_REVISION($Revision$)
AC_PREREQ(2.57)
AC_INIT([remctl], [1.9], [rra@stanford.edu])

AC_PROG_CC
AC_PROG_INSTALL

AC_ARG_WITH([kerberos],
    AC_HELP_STRING([--with-kerberos=DIR],
        [Location of Kerberos headers and libraries]),
    [if test x"$withval" != xno ; then
        KRBLIBS="-L$withval/lib"
        CPPFLAGS="$CPPFLAGS -I$withval/include"
     fi])

AC_SEARCH_LIBS(gethostbyname, nsl)
AC_SEARCH_LIBS(socket, socket, ,
    [AC_CHECK_LIB(nsl, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)])
AC_SEARCH_LIBS(res_search, resolv)
AC_SEARCH_LIBS(crypt, crypt)

KRBLIBS="$KRBLIBS -lgssapi_krb5 -lkrb5 -lk5crypto -lcom_err"

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

dnl Source used by REMCTL_FUNC_SNPRINTF.
define([_REMCTL_FUNC_SNPRINTF_SOURCE],
[[#include <stdio.h>
#include <stdarg.h>

char buf[2];

int
test (char *format, ...)
{
  va_list args;
  int count;

  va_start (args, format);
  count = vsnprintf (buf, sizeof buf, format, args);
  va_end (args);
  return count;
}

int
main ()
{
  return ((test ("%s", "abcd") == 4 && buf[0] == 'a' && buf[1] == '\0'
           && snprintf(NULL, 0, "%s", "abcd") == 4) ? 0 : 1);
}]])

dnl Check for a working snprintf.  Some systems have snprintf, but it doesn't
dnl null-terminate if the buffer isn't large enough or it returns -1 if the
dnl string doesn't fit instead of returning the number of characters that
dnl would have been formatted.
AC_DEFUN([REMCTL_FUNC_SNPRINTF],
[AC_CACHE_CHECK(for working snprintf, remctl_cv_func_snprintf_works,
[AC_TRY_RUN(_REMCTL_FUNC_SNPRINTF_SOURCE(),
            [remctl_cv_func_snprintf_works=yes],
            [remctl_cv_func_snprintf_works=no],
            [remctl_cv_func_snprintf_works=no])])
if test "$remctl_cv_func_snprintf_works" = yes ; then
    AC_DEFINE([HAVE_SNPRINTF], 1,
        [Define if your system has a working snprintf function.])
else
    AC_LIBOBJ([ac_objext])
fi])
REMCTL_FUNC_SNPRINTF

AC_TYPE_SIGNAL

AC_CONFIG_HEADER(config.h)
AC_OUTPUT(Makefile)

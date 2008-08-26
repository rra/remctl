dnl remctl PECL extension for PHP configuration
dnl
dnl Provides additional configuration hooks for the PHP module build system.
dnl This file is used by the phpize frameowrk.
dnl
dnl Originally written by Andrew Mortensen <admorten@umich.edu>, 2008
dnl Copyright 2008 Andrew Mortensen <admorten@umich.edu>
dnl Copyright 2008 Board of Trustees, Leland Stanford Jr. University
dnl
dnl See LICENSE for licensing terms.

PHP_ARG_ENABLE([remctl], [whether to enable remctl PHP extension],
    [AC_HELP_STRING([--enable-remctl], [Enable recmtl PHP extension])], [yes])

dnl The escaping on PHP_ADD_INCLUDE seems to work, and seems to be the only
dnl thing that works, but it makes me nervous.  Needs more testing and maybe a
dnl better approach that will still work for builddir != srcdir builds.
AS_IF([test "$PHP_REMCTL" != no],
    [PHP_NEW_EXTENSION([remctl], [php_remctl.c], [$ext_shared])
     PHP_ADD_INCLUDE([\${ac_top_srcdir}/..])
     LDFLAGS="$LDFLAGS -lremctl"])

PHP_ARG_ENABLE(remctl, whether to enable remctl php extension,
		[--enable-remctl	Enable recmtl php extension],
		yes)

if test "$PHP_REMCTL" != no; then
    AC_DEFINE(HAVE_REMCTL, 1, [remctl support])
    PHP_NEW_EXTENSION(remctl, php_remctl.c, $ext_shared)

    # these are clumsy. we don't need to give the compiler
    # the library search path since the bundle is compiled
    # with "-undefined suppress".
    PHP_ADD_INCLUDE([\${ac_top_srcdir}/..])
    LDFLAGS="$LDFLAGS -lremctl"
fi

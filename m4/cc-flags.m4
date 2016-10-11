dnl Check whether the compiler supports particular flags.
dnl
dnl Provides RRA_PROG_CC_FLAG, which checks whether a compiler supports a
dnl given flag.  If it does, the commands in the second argument are run.  If
dnl not, the commands in the third argument are run.
dnl
dnl Provides RRA_PROG_CC_WARNINGS_FLAGS, which checks whether a compiler
dnl supports a large set of warning flags and sets the WARNINGS_CFLAGS
dnl substitution variable to all of the supported warning flags.  (Note that
dnl this may be too aggressive for some people.)
dnl
dnl Depends on RRA_PROG_CC_CLANG.
dnl
dnl The canonical version of this file is maintained in the rra-c-util
dnl package, available at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
dnl
dnl Copyright 2016 Russ Allbery <eagle@eyrie.org>
dnl Copyright 2006, 2009, 2016
dnl     by Internet Systems Consortium, Inc. ("ISC")
dnl
dnl Permission to use, copy, modify, and distribute this software for any
dnl purpose with or without fee is hereby granted, provided that the above
dnl copyright notice and this permission notice appear in all copies.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
dnl REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
dnl MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
dnl SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
dnl WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
dnl ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
dnl IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

dnl Used to build the result cache name.
AC_DEFUN([_RRA_PROG_CC_FLAG_CACHE],
[translit([rra_cv_compiler_c_$1], [-=], [__])])

dnl Check whether a given flag is supported by the complier.
AC_DEFUN([RRA_PROG_CC_FLAG],
[AC_REQUIRE([AC_PROG_CC])
 AC_MSG_CHECKING([if $CC supports $1])
 AC_CACHE_VAL([_RRA_PROG_CC_FLAG_CACHE([$1])],
    [save_CFLAGS=$CFLAGS
     CFLAGS="$CFLAGS $1"
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [int foo = 0;])],
        [_RRA_PROG_CC_FLAG_CACHE([$1])=yes],
        [_RRA_PROG_CC_FLAG_CACHE([$1])=no])
     CFLAGS=$save_CFLAGS])
 AC_MSG_RESULT([$_RRA_PROG_CC_FLAG_CACHE([$1])])
 AS_IF([test x"$_RRA_PROG_CC_FLAG_CACHE([$1])" = xyes], [$2], [$3])])

dnl Determine the full set of viable warning flags for the current compiler.
dnl
dnl This is based partly on personal preference and is a fairly aggressive set
dnl of warnings.  Desirable warnings that can't be turned on due to other
dnl problems:
dnl
dnl   -Wconversion       http://bugs.debian.org/488884 (htons warnings)
dnl   -Wsign-conversion  Too much noise from ssize_t and flag variables
dnl   -Wstack-protector  Too many false positives from small buffers
dnl
dnl Last checked against gcc 6.1.0 (2016-09-25).  -D_FORTIFY_SOURCE=2 enables
dnl warn_unused_result attribute markings on glibc functions on Linux, which
dnl catches a few more issues.  Add -O because gcc won't find some warnings
dnl without optimization turned on.
dnl
dnl The warnings here are listed in the same order they're listed in the
dnl "Preprocessor Options" and "Warning Options" chapters of the GCC manual.
dnl
dnl Sets WARNINGS_CFLAGS as a substitution variable.
AC_DEFUN([RRA_PROG_CC_WARNINGS_FLAGS],
[AC_REQUIRE([RRA_PROG_CC_CLANG])
 AS_IF([test x"$CLANG" = xyes],
    [WARNINGS_CFLAGS=""
     m4_foreach_w([flag],
        [-Weverything -Wno-padded],
        [RRA_PROG_CC_FLAG(flag,
            [WARNINGS_CFLAGS="${WARNINGS_CFLAGS} flag"])])],
    [WARNINGS_CFLAGS="-g -O -D_FORTIFY_SOURCE=2"
     m4_foreach_w([flag],
        [-fstrict-overflow -fstrict-aliasing -Wcomments -Wendif-labels -Wall
         -Wextra -Wformat=2 -Wformat-signedness -Wnull-dereference -Winit-self
         -Wswitch-enum -Wuninitialized -Wstrict-overflow=5
         -Wmissing-format-attribute -Wduplicated-cond -Wtrampolines
         -Wfloat-equal -Wdeclaration-after-statement -Wshadow -Wpointer-arith
         -Wbad-function-cast -Wcast-align -Wwrite-strings -Wdate-time
         -Wjump-misses-init -Wfloat-conversion -Wlogical-op
         -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes
         -Wnormalized=nfc -Wpacked -Wredundant-decls -Wnested-externs -Winline
         -Wvla -Werror],
        [RRA_PROG_CC_FLAG(flag,
            [WARNINGS_CFLAGS="${WARNINGS_CFLAGS} flag"])])])
 AC_SUBST([WARNINGS_CFLAGS])])

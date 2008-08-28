dnl gput.m4--GPUT libraries and includes
dnl by Jeffrey Hutzelman
dnl based on zephyr.m4, based on kafs.m4, by
dnl Derrick Brashear
dnl from KTH kafs and Arla
dnl $Id$

AC_DEFUN([CMU_GPUT_INC_WHERE1], [
saved_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$saved_CPPFLAGS -I$1"
AC_TRY_COMPILE(
[#include <gput.h>],
[GPUT *foo;],
ac_cv_found_gput_inc=yes,
ac_cv_found_gput_inc=no)
CPPFLAGS=$saved_CPPFLAGS
])

AC_DEFUN([CMU_GPUT_INC_WHERE], [
   for i in $1; do
      AC_MSG_CHECKING(for GPUT headers in $i)
      CMU_GPUT_INC_WHERE1($i)
      CMU_TEST_INCPATH($i, gput)
      if test "$ac_cv_found_gput_inc" = "yes"; then
        ac_cv_gput_where_inc=$i
        AC_MSG_RESULT(found)
        break
      else
        AC_MSG_RESULT(not found)
      fi
    done
])

AC_DEFUN([CMU_GPUT_LIB_WHERE1], [
saved_LIBS=$LIBS
LIBS="$saved_LIBS -L$1 -lgput"
AC_TRY_LINK(,
[gput_open(0, 0);],
[ac_cv_found_gput_lib=yes],
ac_cv_found_gput_lib=no)
LIBS=$saved_LIBS
])

AC_DEFUN([CMU_GPUT_LIB_WHERE], [
   for i in $1; do
      AC_MSG_CHECKING(for GPUT libraries in $i)
      CMU_GPUT_LIB_WHERE1($i)
      dnl deal with false positives from implicit link paths
      CMU_TEST_LIBPATH($i, gput)
      if test "$ac_cv_found_gput_lib" = "yes" ; then
        ac_cv_gput_where_lib=$i
        AC_MSG_RESULT(found)
        break
      else
        AC_MSG_RESULT(not found)
      fi
    done
])

AC_DEFUN([CMU_GPUT], [
AC_REQUIRE([CMU_FIND_LIB_SUBDIR])
AC_ARG_WITH(gput,
        [  --with-gput=PREFIX      Compile with CMU GPUT support],
        [if test "X$with_gput" = "X"; then
                with_gput=yes
        fi])
AC_ARG_WITH(gput-lib,
        [  --with-gput-lib=dir     use gput libraries in dir],
        [if test "$withval" = "yes" -o "$withval" = "no"; then
                AC_MSG_ERROR([No argument for --with-gput-lib])
        fi])
AC_ARG_WITH(gput-include,
        [  --with-gput-include=dir use gput headers in dir],
        [if test "$withval" = "yes" -o "$withval" = "no"; then
                AC_MSG_ERROR([No argument for --with-gput-include])
        fi])

        if test "X$with_gput" != "X"; then
          if test "$with_gput" != "yes" -a "$with_gput" != no; then
            ac_cv_gput_where_lib=$with_gput/$CMU_LIB_SUBDIR
            ac_cv_gput_where_inc=$with_gput/include
          fi
        fi

        if test "$with_gput" != "no"; then 
          if test "X$with_gput_lib" != "X"; then
            ac_cv_gput_where_lib=$with_gput_lib
          fi
          if test "X$ac_cv_gput_where_lib" = "X"; then
            CMU_GPUT_LIB_WHERE(/usr/local/$CMU_LIB_SUBDIR /usr/$CMU_LIB_SUBDIR)
          fi

          if test "X$with_gput_include" != "X"; then
            ac_cv_gput_where_inc=$with_gput_include
          fi
          if test "X$ac_cv_gput_where_inc" = "X"; then
            CMU_GPUT_INC_WHERE(/usr/local/include /usr/include)
          fi
        fi

        AC_MSG_CHECKING(whether to include CMU GPUT support)
        if test "X$ac_cv_gput_where_lib" = "X" -o "X$ac_cv_gput_where_inc" = "X"; then
          ac_cv_found_gput=no
          AC_MSG_RESULT(no)
        else
          ac_cv_found_gput=yes
          AC_MSG_RESULT(yes)
          AC_DEFINE([HAVE_GPUT],,[CMU GPUT library is present])
          GPUT_INC_DIR=$ac_cv_gput_where_inc
          GPUT_LIB_DIR=$ac_cv_gput_where_lib
          GPUT_INC_FLAGS="-I${GPUT_INC_DIR}"
          GPUT_LIB_FLAGS="-L${GPUT_LIB_DIR} -lgput"
	  AC_SUBST(GPUT_INC_FLAGS)
	  AC_SUBST(GPUT_LIB_FLAGS)
          if test "X$RPATH" = "X"; then
                RPATH=""
          fi
          case "${host}" in
            *-*-linux*)
              if test "X$RPATH" = "X"; then
                RPATH="-Wl,-rpath,${GPUT_LIB_DIR}"
              else 
                RPATH="${RPATH}:${GPUT_LIB_DIR}"
              fi
              ;;
            *-*-hpux*)
              if test "X$RPATH" = "X"; then
                RPATH="-Wl,+b${GPUT_LIB_DIR}"
              else 
                RPATH="${RPATH}:${GPUT_LIB_DIR}"
              fi
              ;;
            *-*-irix*)
              if test "X$RPATH" = "X"; then
                RPATH="-Wl,-rpath,${GPUT_LIB_DIR}"
              else 
                RPATH="${RPATH}:${GPUT_LIB_DIR}"
              fi
              ;;
            *-*-solaris2*)
              if test "$ac_cv_prog_gcc" = yes; then
                if test "X$RPATH" = "X"; then
                  RPATH="-Wl,-R${GPUT_LIB_DIR}"
                else 
                  RPATH="${RPATH}:${GPUT_LIB_DIR}"
                fi
              else
                RPATH="${RPATH} -R${GPUT_LIB_DIR}"
              fi
              ;;
          esac
          AC_SUBST(RPATH)
        fi
        ])

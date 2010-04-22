dnl $Id$
dnl config.m4 for extension odbx

AC_DEFUN([ODBX_LIB_CHK], [
  str="$ODBX_DIR/$1/lib$ODBX_LIBNAME.*"
  for j in `echo $str`; do
    if test -r $j; then
      ODBX_LIB_DIR=$ODBX_DIR/$1
      break 2
    fi
  done
])

PHP_ARG_ENABLE(odbx, whether to enable odbx support,
dnl Make sure that the comment is aligned:
[  --enable-odbx           Enable odbx support])

if test -z "$PHP_ODBX_DIR"; then
PHP_ARG_WITH(odbx-dir, for the location of OpenDBX,
[  --with-odbx-dir[=DIR]   odbx: Set the path to OpenDBX install prefix.], no, no)
fi

if test "$PHP_ODBX" != "no"; then
  AC_DEFINE(HAVE_ODBX, 1, [Whether you have OpenDBX])

  ODBX_DIR=
  ODBX_INC_DIR=

  for i in $PHP_ODBX /usr/local /usr; do
    if test -r $i/include/opendbx/odbx.h; then
      ODBX_DIR=$i
      ODBX_INC_DIR=$i/include/opendbx
      break
    elif test -r $i/include/odbx.h; then
      ODBX_DIR=$i
      ODBX_INC_DIR=$i/include
      break
    fi
  done

  if test -z "$ODBX_DIR"; then
    AC_MSG_ERROR([Cannot find OpenDBX header files under $PHP_ODBX.
Note that the OpenDBX client library is not bundled anymore!])
  fi

  ODBX_LIBNAME=opendbx

  dnl for compat with PHP 4 build system
  if test -z "$PHP_LIBDIR"; then
    PHP_LIBDIR=lib
  fi

  for i in $PHP_LIBDIR $PHP_LIBDIR/opendbx; do
    ODBX_LIB_CHK($i)
  done

  if test -z "$ODBX_LIB_DIR"; then
    AC_MSG_ERROR([Cannot find lib$ODBX_LIBNAME under $ODBX_DIR.
Note that the OpenDBX client library is not bundled anymore!])
  fi

  PHP_CHECK_LIBRARY($ODBX_LIBNAME, odbx_init, [ ],
  [
      PHP_ADD_LIBRARY(z,, ODBX_SHARED_LIBADD)
  ], [
    -L$ODBX_LIB_DIR
  ])

  PHP_ADD_LIBRARY_WITH_PATH($ODBX_LIBNAME, $ODBX_LIB_DIR, ODBX_SHARED_LIBADD)
  PHP_ADD_INCLUDE($ODBX_INC_DIR)


  PHP_NEW_EXTENSION(odbx, odbx.c, $ext_shared)


  ODBX_MODULE_TYPE=external
  ODBX_LIBS="-L$ODBX_LIB_DIR -l$ODBX_LIBNAME $ODBX_LIBS"
  ODBX_INCLUDE=-I$ODBX_INC_DIR

  PHP_SUBST(ODBX_SHARED_LIBADD)
  PHP_SUBST_OLD(ODBX_MODULE_TYPE)
  PHP_SUBST_OLD(ODBX_LIBS)
  PHP_SUBST_OLD(ODBX_INCLUDE)


  dnl this is needed to build the extension with phpize and -Wall

  if test "$PHP_DEBUG" = "yes"; then
    CFLAGS="$CFLAGS -Wall"
  fi

fi

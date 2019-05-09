# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2010-2011 Nikolai Kondrashov
#
# This file is part of usbhid-dump.

AC_PREREQ(2.61)
AC_INIT([usbhid-dump], [1.4])
AC_CONFIG_AUX_DIR([auxdir])
AM_INIT_AUTOMAKE([1.9 -Wall foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# To have empty CFLAGS instead of undefined and '-g -O2' by default
CFLAGS=$CFLAGS
CFLAGS="-Os -Wall $CFLAGS"
ABS_SRCDIR=`cd ${srcdir} ; pwd`
ABS_BUILDDIR=`pwd`
CPPFLAGS="-I${ABS_BUILDDIR} -I${ABS_BUILDDIR}/include -DNDEBUG $CPPFLAGS"
if test "$ABS_SRCDIR" != "$ABS_BUILDDIR"; then
    CPPFLAGS="-I${ABS_SRCDIR}/include $CPPFLAGS"
fi


#
# Checks for programs.
#
AC_PROG_CC
AC_PROG_INSTALL
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL

#
# Checks for libraries.
#
PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.0)
CFLAGS="$CFLAGS $LIBUSB_CFLAGS"
LIBS="$LIBS $LIBUSB_LIBS"

#
# Checks for features
#
AC_ARG_ENABLE(
    debug,
    AS_HELP_STRING([--enable-debug], [enable debugging features]),
    [], [enable_debug="no"])

AC_ARG_ENABLE(
    tests-install,
    AS_HELP_STRING([--enable-tests-install], [enable installation of tests]),
    [], [enable_tests_install="no"])

# Output features to preprocessor and compiler
if test "$enable_debug" = "yes"; then
    CPPFLAGS="$CPPFLAGS -UNDEBUG"
    CFLAGS="$CFLAGS -Wextra -Werror -g -O0"
fi

#
# Checks for header files.
#

#
# Checks for typedefs, structures, and compiler characteristics.
#

#
# Checks for declarations
#

#
# Checks for library functions.
#

#
# Output
#
AC_CONFIG_FILES([Makefile
                 include/Makefile
                 include/uhd/Makefile
                 src/Makefile])
AC_OUTPUT

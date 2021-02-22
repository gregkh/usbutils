# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2010-2011 Nikolai Kondrashov
#
# This file is part of usbhid-dump.

AC_PREREQ(2.61)
AC_INIT([usbhid-dump], [1.4])
AM_INIT_AUTOMAKE([1.9 -Wall foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE

AC_USE_SYSTEM_EXTENSIONS

AC_CONFIG_HEADERS([config.h])

# To have empty CFLAGS instead of undefined and '-g -O2' by default
CFLAGS=$CFLAGS
CFLAGS="-Os -Wall $CFLAGS"
ABS_SRCDIR=`cd ${srcdir} ; pwd`
ABS_BUILDDIR=`pwd`
CPPFLAGS="-I${ABS_BUILDDIR} -DNDEBUG $CPPFLAGS"


#
# Checks for programs.
#

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
AC_CHECK_FUNCS(libusb_set_option)

#
# Output
#
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

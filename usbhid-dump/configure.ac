#
# Copyright (C) 2010 Nikolai Kondrashov
#
# This file is part of usbhid-dump.
#
# Usbhid-dump is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Usbhid-dump is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with usbhid-dump; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([usbhid-dump], [1.2])
AC_CONFIG_AUX_DIR([auxdir])
AM_INIT_AUTOMAKE([1.9 -Wall foreign])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

# To have empty CFLAGS instead of undefined and '-g -O2' by default
CFLAGS=$CFLAGS
CFLAGS="-Os -Wall $CFLAGS"
ABS_SRCDIR=`cd ${srcdir} ; pwd`
ABS_BUILDDIR=`pwd`
CPPFLAGS="-I${ABS_BUILDDIR} -I${ABS_BUILDDIR}/include -D_GNU_SOURCE -DNDEBUG $CPPFLAGS"
if test "$ABS_SRCDIR" != "$ABS_BUILDDIR"; then
    CPPFLAGS="-I${ABS_SRCDIR}/include $CPPFLAGS"
fi


#
# Checks for programs.
#
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

#
# Checks for libraries.
#
AC_CHECK_LIB(
    [usb-1.0], [libusb_init], [],
    [AC_MSG_FAILURE([libusb is required, but was not found, abort.
                     Try installing libusb-1.0-0-dev or libusb1-devel.])])

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
                 lib/Makefile
                 src/Makefile])
AC_OUTPUT

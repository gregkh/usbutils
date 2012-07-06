AC_INIT([usbutils],
	[006],
	[linux-usb@vger.kernel.org])
AC_PREREQ(2.60)
AM_INIT_AUTOMAKE([check-news foreign 1.9 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_PREFIX_DEFAULT([/usr])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AC_CHECK_HEADERS([byteswap.h])

AC_ARG_ENABLE(zlib,
	AS_HELP_STRING(--disable-zlib,disable support for zlib))

HAVE_ZLIB=no
AS_IF([test "x$enable_zlib" != "xno"],
	[AC_CHECK_LIB(z, inflateEnd, HAVE_ZLIB=yes)])
AM_CONDITIONAL(HAVE_ZLIB, [test "$HAVE_ZLIB" = "yes"])

AC_ARG_ENABLE(usbids,
	AS_HELP_STRING(--disable-usbids, [disable installing usb.ids @<:@default=install@:>@]))
AM_CONDITIONAL([INSTALL_USBIDS], [test "x$enable_usbids" != "xno"])

PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.0)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
])
AC_CONFIG_SUBDIRS([usbhid-dump])

AC_OUTPUT
AC_MSG_RESULT([
	usbutils $VERSION
	=============

	prefix:                 ${prefix}
	datarootdir:            ${datarootdir}
	datadir:                ${datadir}
	mandir:                 ${mandir}

	usb.ids:                ${datadir}/usb.ids
	zlib:                   ${USE_ZLIB}

	compiler:               ${CC}
	cflags:                 ${CFLAGS}
	ldflags:                ${LDFLAGS}
])

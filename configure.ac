AC_INIT([usbutils],
	[0.84],
	[linux-usb@vger.kernel.org])
AC_PREREQ(2.60)
AM_INIT_AUTOMAKE([check-news foreign 1.9 dist-bzip2])
AC_PREFIX_DEFAULT([/usr])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

USE_ZLIB=yes
AC_ARG_ENABLE(zlib,
	AS_HELP_STRING(--disable-zlib,disable support for zlib),
	[
		if eval "test x$enable_zlib = xno"; then
			USE_ZLIB=
		fi
	])
if test "$USE_ZLIB" = "yes" ; then
	AC_CHECK_LIB(z, inflateEnd)
	if test "${ac_cv_lib_z_inflateEnd}" = "yes" ; then
		HAVE_ZLIB="yes"
	fi
fi
AM_CONDITIONAL(HAVE_ZLIB, test x$HAVE_ZLIB = xyes)

PKG_CHECK_MODULES(LIBUSB, libusb >= 0.1.12)
AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
])

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

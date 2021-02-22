#!/bin/sh -e
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2009,2010 Greg Kroah-Hartman <gregkh@suse.de>

if test ! -f usbhid-dump/bootstrap; then
git submodule update --init --recursive
fi

cd usbhid-dump
./bootstrap
cd ..

#gtkdocize
autoreconf --install --symlink

MYCFLAGS="-g -Wall \
-Wmissing-declarations -Wmissing-prototypes \
-Wnested-externs -Wpointer-arith \
-Wpointer-arith -Wsign-compare -Wchar-subscripts \
-Wstrict-prototypes -Wshadow \
-Wformat=2 -Wtype-limits"

case "$CFLAGS" in
	*-O[0-9]*)
		;;
	*)
		MYCFLAGS="$MYCFLAGS -O2"
		;;
esac

libdir() {
	echo $(cd $1/$(gcc -print-multi-os-directory); pwd)
}

args="--prefix=/usr \
--sysconfdir=/etc \
--sbindir=/sbin \
--libdir=$(libdir /usr/lib) \
--with-rootlibdir=$(libdir /lib) \
--libexecdir=/lib/udev \
--with-selinux \
--enable-gtk-doc"

export CFLAGS="$CFLAGS $MYCFLAGS"
./configure $args $@

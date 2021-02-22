# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (c) 2004 Thomas Sailer <sailer@ife.ee.ethz.ch>
# Copyright (c) 2005-2006 David Brownell <dbrownell@users.sourceforge.net>
# Copyright (c) 2009 Kay Sievers <kay.sievers@vrfy.org>
# Copyright (c) 2009 Greg Kroah-Hartman <gregkh@suse.de>

SUBDIRS = \
	usbhid-dump

AM_LDFLAGS = \
	-Wl,--as-needed

data_DATA =

bin_PROGRAMS = \
	lsusb

sbin_SCRIPTS =

bin_SCRIPTS = \
	usb-devices \
	lsusb.py

noinst_PROGRAMS = \
	usbreset

lsusb_SOURCES = \
	lsusb.c lsusb.h \
	lsusb-t.c \
	list.h \
	desc-defs.c desc-defs.h \
	desc-dump.c desc-dump.h \
	names.c names.h \
	sysfs.c sysfs.h \
	usb-spec.h \
	usbmisc.c usbmisc.h

lsusb_CPPFLAGS = \
	$(AM_CPPFLAGS) $(LIBUSB_CFLAGS) $(UDEV_CFLAGS)

lsusb_LDADD = \
	$(LIBUSB_LIBS) \
	$(UDEV_LIBS)

usbreset_SOURCES = \
	usbreset.c

man_MANS = \
	lsusb.8	\
	usbhid-dump.8 \
	usb-devices.1

EXTRA_DIST = \
	lsusb.8.in \
	usbhid-dump.8.in \
	usb-devices.1.in \
	usb-devices \
	lsusb.py.in \
	usbreset.c \
	LICENSES/GPL-2.0-only.txt \
	LICENSES/GPL-3.0-only.txt

lsusb.py: $(srcdir)/lsusb.py.in
	sed 's|VERSION|$(VERSION)|g;s|@usbids@|$(datadir)/usb.ids|g' $< >$@
	chmod 755 $@

lsusb.8: $(srcdir)/lsusb.8.in
	sed 's|VERSION|$(VERSION)|g' $< >$@

usbhid-dump.8: $(srcdir)/usbhid-dump.8.in
	sed 's|VERSION|$(VERSION)|g' $< >$@

usb-devices.1: $(srcdir)/usb-devices.1.in
	sed 's|VERSION|$(VERSION)|g' $< >$@

DISTCLEANFILES = \
	lsusb.py \
	lsusb.8 \
	usb-devices.1 \
	usbhid-dump.8

distclean-local:
	rm -rf autom4te.cache

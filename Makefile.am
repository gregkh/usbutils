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

lsusb_SOURCES = \
	lsusb.c lsusb.h \
	lsusb-t.c \
	list.h \
	names.c names.h \
	usb-spec.h \
	usbmisc.c usbmisc.h

lsusb_CPPFLAGS = \
	$(AM_CPPFLAGS) $(LIBUSB_CFLAGS) $(UDEV_CFLAGS) \
	-DDATADIR=\"$(datadir)\"

lsusb_LDADD = \
	$(LIBUSB_LIBS) \
	$(UDEV_LIBS)

man_MANS = \
	lsusb.8	\
	usb-devices.1

EXTRA_DIST = \
	lsusb.8.in \
	usb-devices.1.in \
	usb-devices \
	lsusb.py \
	usbutils.pc.in

lsusb.8: $(srcdir)/lsusb.8.in
	sed 's|VERSION|$(VERSION)|g;s|@usbids@|$(datadir)/usb.ids|g' $< >$@

usb-devices.1: $(srcdir)/usb-devices.1.in
	sed 's|VERSION|$(VERSION)|g' $< >$@

pkgconfigdir = $(datarootdir)/pkgconfig
pkgconfig_DATA = usbutils.pc

usbutils.pc: $(srcdir)/usbutils.pc.in
	sed 's|@usbids@|$(datadir)/usb.ids|g;s|@VERSION[@]|$(VERSION)|g' $< >$@

DISTCLEANFILES = \
	lsusb.8 \
	usb-devices.1 \
	usbutils.pc

distclean-local:
	rm -rf autom4te.cache

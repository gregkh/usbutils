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
	usbmisc.c usbmisc.h

lsusb_CPPFLAGS = \
	$(AM_CPPFLAGS) $(LIBUSB_CFLAGS) \
	-DDATADIR=\"$(datadir)\"

lsusb_LDADD = \
	$(LIBUSB_LIBS)

if HAVE_ZLIB
lsusb_CPPFLAGS += -DHAVE_LIBZ
lsusb_LDADD += -lz
endif

man_MANS = \
	lsusb.8	\
	usb-devices.1

EXTRA_DIST = \
	usb.ids \
	update-usbids.sh.in \
	lsusb.8.in \
	usb-devices.1.in \
	usb-devices \
	lsusb.py \
	usbutils.pc.in

if INSTALL_USBIDS
data_DATA += usb.ids

if HAVE_ZLIB
data_DATA += usb.ids.gz
endif

sbin_SCRIPTS += update-usbids.sh

usb.ids.gz: $(srcdir)/usb.ids
	gzip -c -9 $< > $@

update-usbids.sh: $(srcdir)/update-usbids.sh.in
	sed 's|@usbids@|$(datadir)/usb.ids|g' $< >$@
	chmod 755 $@
endif

lsusb.8: $(srcdir)/lsusb.8.in
	sed 's|VERSION|$(VERSION)|g;s|@usbids@|$(datadir)/usb.ids|g' $< >$@

usb-devices.1: $(srcdir)/usb-devices.1.in
	sed 's|VERSION|$(VERSION)|g' $< >$@

pkgconfigdir = $(datarootdir)/pkgconfig
pkgconfig_DATA = usbutils.pc

usbutils.pc: $(srcdir)/usbutils.pc.in
	sed 's|@usbids@|$(datadir)/usb.ids|g;s|@VERSION[@]|$(VERSION)|g' $< >$@

DISTCLEANFILES = \
	usb.ids.gz \
	lsusb.8 \
	usb-devices.1 \
	update-usbids.sh \
	usbutils.pc

distclean-local:
	rm -rf autom4te.cache

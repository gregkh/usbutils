AM_LDFLAGS = \
	-Wl,--as-needed

data_DATA = \
	usb.ids
if HAVE_ZLIB
data_DATA += usb.ids.gz
endif

sbin_PROGRAMS = \
	lsusb

sbin_SCRIPTS = \
	update-usbids.sh \
	usb-devices

lsusb_SOURCES = \
	lsusb.c \
	lsusb-t.c \
	list.h \
	names.c names.h \
	devtree.c devtree.h \
	usbmisc.c usbmisc.h

lsusb_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DDATADIR=\"$(datadir)\"

lsusb_LDADD = \
	$(LIBUSB_LIBS)

man_MANS = \
	lsusb.8	\
	usb-devices.8

EXTRA_DIST = \
	usb.ids \
	update-usbids.sh.in \
	lsusb.8.in \
	usb-devices.8.in \
	usb-devices \
	usbutils.pc.in

usb.ids.gz: $(srcdir)/usb.ids
	gzip -c -9 $< > $@

update-usbids.sh: $(srcdir)/update-usbids.sh.in
	sed 's|@usbids@|$(datadir)/usb.ids|g' $< >$@
	chmod 755 $@

lsusb.8: $(srcdir)/lsusb.8.in
	sed 's|@usbids@|$(datadir)/usb.ids|g' $< >$@

usb-devices.8: $(srcdir)/usb-devices.8.in
	sed 's|VERSION|$(VERSION)|g' $< >$@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = usbutils.pc

usbutils.pc: $(srcdir)/usbutils.pc.in
	sed 's|@usbids@|$(datadir)/usb.ids|g;s|@VERSION[@]|$(VERSION)|g' $< >$@

DISTCLEANFILES = \
	usb.ids.gz \
	lsusb.8 \
	usb-devices.8 \
	update-usbids.sh \
	usbutils.pc

distclean-local:
	rm -rf autom4te.cache

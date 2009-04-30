AM_LDFLAGS = \
	-Wl,--as-needed

data_DATA = \
	usb.ids
if HAVE_ZLIB
	usb.ids.gz
endif

sbin_PROGRAMS = \
	lsusb

sbin_SCRIPTS = \
	update-usbids.sh

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
	lsusb.8

EXTRA_DIST = \
	usb.ids \
	update-usbids.sh.in \
	lsusb.8.in

update-usbids.sh: $(srcdir)/update-usbids.sh.in
	sed 's|@usbids@|$(datadir)/usb.ids|g' $< >$@
	chmod 755 $@

lsusb.8: $(srcdir)/lsusb.8.in
	sed 's|@usbids@|$(datadir)/usb.ids|g' $< >$@

usb.ids.gz: usb.ids
	gzip -c -9 usb.ids > usb.ids.gz

DISTCLEANFILES = \
	usb.ids.gz \
	lsusb.8 \
	update-usbids.sh

distclean-local:
	rm -rf autom4te.cache

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

update-usbids.sh: update-usbids.sh.in
	sed 's|@usbids@|$(datadir)/usb.ids|' $< >$@
	chmod 755 $@

lsusb.8: lsusb.8.in
	sed 's|@usbids@|$(datadir)/usb.ids|' $< >$@

usb.ids.gz: usb.ids
	gzip -c -9 usb.ids > usb.ids.gz

clean-local:
	rm -f usb.ids.gz
	rm -f lsusb.8
	rm -f update-usbids.sh

distclean-local:
	rm -rf autom4te.cache

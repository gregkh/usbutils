AM_LDFLAGS = \
	-Wl,--as-needed

data_DATA = \
	usb.ids
if HAVE_ZLIB
	usb.ids.gz
endif

sbin_PROGRAMS = \
	lsusb

lsusb_SOURCES = \
	lsusb.c \
	list.h \
	names.c names.h \
	devtree.c devtree.h \
	usbmisc.c usbmisc.h

lsusb_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DDATADIR=\"$(datadir)\"

lsusb_LDADD = \
	$(LIBUSB_LIBS)

dist_man_MANS = \
	lsusb.8

EXTRA_DIST = \
	usb.ids

usb.ids.gz: usb.ids
	gzip -c -9 usb.ids > usb.ids.gz

clean-local:
	rm -f usb.ids.gz

distclean-local:
	rm -rf autom4te.cache

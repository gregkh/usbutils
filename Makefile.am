# Process this file with 'automake' to create 'Makefile.in'

AM_CPPFLAGS = -DUSBIDS_DIR='"@datadir@"'

AM_CFLAGS = -Wall -W -Wunused

sbin_PROGRAMS = lsusb

lsusb_SOURCES = lsusb.c \
	names.c names.h \
	devtree.c devtree.h list.h \
	usbmisc.c usbmisc.h

man_MANS = lsusb.8

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)
if HAVE_ZLIB
	gzip -c -9 $(srcdir)/usb.ids > $(DESTDIR)$(datadir)/usb.ids.gz
else
	$(INSTALL_DATA) $(srcdir)/usb.ids $(DESTDIR)$(datadir)/usb.ids
endif

EXTRA_DIST = $(man_MANS) usb.ids update-usbids.sh

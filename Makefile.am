# Process this file with 'automake' to create 'Makefile.in'

AM_CPPFLAGS = -DUSBIDS_FILE='"@datadir@/usb.ids"'

AM_CFLAGS = -Wall -W -Wunused

sbin_PROGRAMS = lsusb

lsusb_SOURCES = lsusb.c \
	names.c names.h \
	devtree.c devtree.h list.h \
	usbmisc.c usbmisc.h

data_DATA = usb.ids

man_MANS = lsusb.8

# usbmodules is needed only with 2.4 based hotplugging, and is DEPRECATED 

EXTRA_PROGRAMS = usbmodules

usbmodules_SOURCES = usbmodules.c usbmodules.h \
	usbmisc.c usbmisc.h

if USBMODULES
    sbin_PROGRAMS += usbmodules
    man_MANS += usbmodules.8
endif

EXTRA_DIST = $(man_MANS) $(data_DATA) usbmodules.8 update-usbids.sh

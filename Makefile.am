SUBDIRS =

INCLUDES = 

sbin_PROGRAMS = lsusb usbmodules

noinst_HEADERS = names.h usbdevice_fs.h devtree.h list.h usbmodules.h usbmisc.h

lsusb_SOURCES = lsusb.c names.c devtree.c usbmisc.c
lsusb_LDADD = @LIBOBJS@

usbmodules_SOURCES = usbmodules.c usbmisc.c
usbmodules_LDADD = @LIBOBJS@

data_DATA = usb.ids

man_MANS = lsusb.8 usbmodules.8

EXTRA_DIST = $(man_MANS) $(data_DATA) usbutils.spec getopt.h getopt.c getopt1.c

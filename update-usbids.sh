#!/bin/sh

# see also update-pciids.sh (fancier)

set -e
SRC="http://www.linux-usb.org/usb.ids"
DEST=usb.ids

if which wget >/dev/null ; then
	DL="wget -O $DEST.new $SRC"
elif which lynx >/dev/null ; then
	DL="eval lynx -source $SRC >$DEST.new"
else
	echo >&2 "update-usbids: cannot find wget nor lynx"
	exit 1
fi

if ! $DL ; then
	echo >&2 "update-usbids: download failed"
	rm -f $DEST.new
	exit 1
fi

if ! grep >/dev/null "^C " $DEST.new ; then
	echo >&2 "update-usbids: missing class info, probably truncated file"
	exit 1
fi

if [ -f $DEST ] ; then
	mv $DEST $DEST.old
	# --reference is supported only by chmod from GNU file, so let's ignore any errors
	chmod -f --reference=$DEST.old $DEST.new 2>/dev/null || true
fi
mv $DEST.new $DEST

echo "Done."

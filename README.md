<!---
SPDX-License-Identifier: GPL-2.0+
Copyright (c) 2018 Greg Kroah-Hartman <gregkh@linuxfoundation.org>
-->
# usbutils

This is a collection of USB tools for use on Linux and BSD systems to
query what type of USB devices are connected to the system.  This is to
be run on a USB host (i.e. a machine you plug USB devices into), not on
a USB device (i.e. a device you plug into a USB host.)

## Building and installing

Note, usbutils depends on libusb, be sure that library is properly
installed first.

To work with the "raw" repo, after cloning it just do:

	./autogen.sh

Or if you like doing things "by hand" you can try the following:

Get the usbhid-dump git submodule:

	git submodule init
	git submodule update

Initialize autobuild with:

	autoreconf --install --symlink

Configure the project with:

	./configure

Build everything with:

	make

Install it, if you really want to, with:

	make install

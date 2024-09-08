#!/usr/bin/env -S just --justfile
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2024 Greg Kroah-Hartman <gregkh@linuxfoundation.org>
#
# justfile to help remember how to do a release to kernel.org
# For only to be used by the maintainer when doing a new release.
#

# show the list of options
_help:
	@just --list


# Do a release by signing the tarball and uploading to kernel.org
@release *version='':
	# just echo stuff for now, need to turn this into a real script...
	echo "$ gpg -a -b usbutils-014.tar"
	echo "$ kup --host=git@gitolite.kernel.org --subcmd=kup-server put usbutils-014.tar usbutils-014.tar.asc  /pub/linux/utils/usb/usbutils/usbutils-014.tar.gz"

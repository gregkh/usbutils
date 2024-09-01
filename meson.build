# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2024 Greg Kroah-Hartman <gregkh@linuxfoundation.org>

project('usbutils', 'c', version: '018')

pkg = import('pkgconfig')

# Compiler stuff
cc = meson.get_compiler('c')

# Note, when building with autotools, the default autogen.sh script had these C
# flags set, we should probably also enable them, if not more, here as well in
# the future.
  #MYCFLAGS="-g -Wall \
  # -Wmissing-declarations -Wmissing-prototypes \
  #-Wnested-externs -Wpointer-arith \
  #-Wpointer-arith -Wsign-compare -Wchar-subscripts \
  #-Wstrict-prototypes -Wshadow \
  #-Wformat=2 -Wtype-limits"

# Configuration information
config = configuration_data()
config.set_quoted('PACKAGE_NAME', meson.project_name())
config.set_quoted('VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: config)

# Build usbhid-dump in a subdir
subdir('usbhid-dump')


#####################
# man page generation
#####################
mandir = join_paths(get_option('prefix'), get_option('mandir'))
man1dir = join_paths(mandir, 'man1')
man8dir = join_paths(mandir, 'man8')

# This can probably be cleaned up a lot to work on a list
# instead of doing it by hand.
usb_devices_man = configure_file(input: files('usb-devices.1.in'), output: 'usb-devices.1', configuration: config)
lsusb_py_man    = configure_file(input: files('lsusb.py.1.in'),    output: 'lsusb.py.1',    configuration: config)
install_man([usb_devices_man, lsusb_py_man], install_dir: man1dir)

lsusb_man       = configure_file(input: files('lsusb.8.in'),       output: 'lsusb.8',       configuration: config)
usbhid_dump_man = configure_file(input: files('usbhid-dump.8.in'), output: 'usbhid-dump.8', configuration: config)
install_man([lsusb_man, usbhid_dump_man], install_dir: man8dir)


##########################
# lsusb build instructions
##########################
lsusb_sources = [
  'desc-defs.c',
  'desc-defs.h',
  'desc-dump.c',
  'desc-dump.h',
  'list.h',
  'lsusb-t.c',
  'lsusb.c',
  'lsusb.h',
  'names.c',
  'names.h',
  'sysfs.c',
  'sysfs.h',
  'usb-spec.h',
  'usbmisc.c',
  'usbmisc.h',
]

libudev = dependency('libudev', version: '>= 196')
libusb = dependency('libusb-1.0', version: '>= 1.0.12')

executable('lsusb', lsusb_sources, dependencies: [libusb, libudev], install: true)


##############################
# usbreset build instructions
##############################
usbreset_sources = [
  'usbreset.c'
]

usbreset_man = configure_file(input: files('usbreset.1.in'), output: 'usbreset.1', configuration: config)

# By default, usbreset does not get installed as it could cause problems, it's
# in the repo for those that wish to try it out.
executable('usbreset', usbreset_sources, install: false)


################################
# usb-devices build instructions
################################
usb_devices_sources = [
  'usb-devices'
]
# Hack as this is not something to compile, but rather we can just copy it.
install_data(usb_devices_sources, install_dir: get_option('bindir'), install_mode: 'rwxr-xr-x')


#############################
# lsusb.py build instructions
#############################
lsusb_py_conf = configuration_data()
lsusb_py_conf.set('VERSION', meson.project_version())
lsusb_py_conf.set('DATADIR', get_option('datadir'))

lsusb_py = configure_file(input: files('lsusb.py.in'), output: 'lsusb.py', configuration: lsusb_py_conf)
# Also a hack, like was done for usb-devices, as this is "just" a script and
# doesn't need to be compiled.
install_data(lsusb_py, install_dir: get_option('bindir'), install_mode: 'rwxr-xr-x')

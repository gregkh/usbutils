# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2024 Greg Kroah-Hartman <gregkh@linuxfoundation.org>

project(
  'usbutils',
  'c',
  version: '018',
  license: ['GPL-2.0-only', 'GPL-2.0-or-later','GPL-2.0-or-later'],
  meson_version : '>=0.60.0',
  default_options : ['c_std=gnu99', 'warning_level=2', 'prefix=/usr']
)

pkg = import('pkgconfig')

# Compiler stuff
cc = meson.get_compiler('c')

add_project_arguments(
  cc.get_supported_arguments([
    '-Wbad-function-cast',
    '-Wcast-align',
    '-Wchar-subscripts',
    '-Wempty-body',
    '-Wformat',
    '-Wformat=2',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wformat-y2k',
    '-Winit-self',
    '-Winline',
    '-Wint-conversion',
    '-Wmissing-declarations',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-prototypes',
    '-Wmissing-noreturn',
    '-Wnested-externs',
    '-Wold-style-definition',
    '-Wold-style-declaration',
    '-Wpointer-arith',
    '-Wredundant-decls',
    '-Wshadow',
    '-Wsign-compare',
    #'-Wsuggest-attribute=const',
    #'-Wsuggest-attribute=noreturn',
    #'-Wsuggest-attribute=malloc',
    #'-Wsuggest-attribute=returns_nonnull',
    '-Wstrict-prototypes',
    '-Wswitch',
    '-Wtype-limits',
    '-Wundef',
    '-Wuninitialized',
    '-Wunused',
    '-Wunused-variable',
    '-Wvla',
    '-Wwrite-strings',
    '-fdiagnostics-color=auto',
    # warnings disabled on purpose
    '-Wno-unused-parameter',
    '-Wno-unused-function',
    '-Wno-deprecated-declarations',
    # should be removed and the code fixed
    '-Wno-discarded-qualifiers',
    '-Wno-incompatible-pointer-types-discards-qualifiers',
    '-Wno-missing-field-initializers',
  ]),
  language: 'c',
)

# Configuration information
config = configuration_data()
config.set_quoted('PACKAGE_NAME', meson.project_name())
config.set_quoted('VERSION', meson.project_version())
config_h = configure_file(output: 'config.h', configuration: config)

add_project_arguments('-include', 'config.h', language : 'c')

# Build usbhid-dump in a subdir
subdir('usbhid-dump')


#####################
# man page generation
#####################
mandir = join_paths(get_option('prefix'), get_option('mandir'))
man1dir = join_paths(mandir, 'man1')
man8dir = join_paths(mandir, 'man8')

install_man(files('usb-devices.1', 'lsusb.py.1'), install_dir: man1dir)
install_man(files('lsusb.8', 'usbhid-dump.8'), install_dir: man8dir)


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
libusb = dependency('libusb-1.0', version: '>= 1.0.22')

executable('lsusb', lsusb_sources, dependencies: [libusb, libudev], install: true)


##############################
# usbreset build instructions
##############################
usbreset_sources = [
  'usbreset.c'
]

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
# Also a hack, like was done for usb-devices, as this is "just" a script and
# doesn't need to be compiled.
install_data(files('lsusb.py'), install_dir: get_option('bindir'), install_mode: 'rwxr-xr-x')

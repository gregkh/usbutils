# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) 2024 Greg Kroah-Hartman <gregkh@linuxfoundation.org>

configure_file(output: 'config.h', configuration: config)

sources = [
  'dev.c',
  'dev.h',
  'dev_list.c',
  'dev_list.h',
  'iface.c',
  'iface.h',
  'iface_list.c',
  'iface_list.h',
  'misc.h',
  'usbhid-dump.c',
]

libusb = dependency('libusb-1.0', version: '>= 1.0.12')
executable('usbhid-dump', sources, dependencies: libusb, install: true)

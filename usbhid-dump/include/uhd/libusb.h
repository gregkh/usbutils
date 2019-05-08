// SPDX-License-Identifier: GPL-2.0+
/*
 * usbhid-dump - libusb API extensions
 *
 * Copyright (C) 2010-2011 Nikolai Kondrashov <spbnick@gmail.com>
 */

#ifndef __UHD_LIBUSB_H__
#define __UHD_LIBUSB_H__

#include <libusb.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_LIBUSB_STRERROR
extern const char *libusb_strerror(enum libusb_error err);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __UHD_LIBUSB_H__ */

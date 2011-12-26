/** @file
 * @brief usbhid-dump - libusb API extensions
 *
 * Copyright (C) 2010-2011 Nikolai Kondrashov
 *
 * This file is part of usbhid-dump.
 *
 * Usbhid-dump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Usbhid-dump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with usbhid-dump; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @author Nikolai Kondrashov <spbnick@gmail.com>
 *
 * @(#) $Id$
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

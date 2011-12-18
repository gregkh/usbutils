/** @file
 * @brief usbhid-dump - device list
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

#ifndef __UHD_DEV_LIST_H__
#define __UHD_DEV_LIST_H__

#include <stddef.h>
#include "uhd/dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if a device list is valid.
 *
 * @param list  Device list to check.
 *
 * @return True if the device list is valid, false otherwise.
 */
extern bool uhd_dev_list_valid(const uhd_dev *list);

/**
 * Check if a device list is empty.
 *
 * @param list  Device list to check.
 *
 * @return True if the device list is empty, false otherwise.
 */
static inline bool
uhd_dev_list_empty(const uhd_dev *list)
{
    return list == NULL;
}

/**
 * Calculate length of a device list.
 *
 * @param list  The list to calculate length of.
 *
 * @return The list length.
 */
extern size_t uhd_dev_list_len(const uhd_dev *list);

/**
 * Close every device in a device list.
 *
 * @param list  The device list to close.
 */
extern void uhd_dev_list_close(uhd_dev *list);

/**
 * Iterate over a device list.
 *
 * @param _dev    Loop device variable.
 * @param _list     Device list to iterate over.
 */
#define UHD_DEV_LIST_FOR_EACH(_dev, _list) \
    for (_dev = _list; _dev != NULL; _dev = _dev->next)

/**
 * Open a list of devices optionally matching bus number/device address and
 * vendor/product IDs.
 *
 * @param ctx       Libusb context.
 * @param bus_num   Bus number, or 0 for any bus.
 * @param dev_addr  Device address, or 0 for any address.
 * @param vid       Vendor ID, or 0 for any vendor.
 * @param pid       Product ID, or 0 for any product.
 * @param plist     Location for the resulting device list head; could be
 *                  NULL.
 *
 * @return Libusb error code.
 */
extern enum libusb_error uhd_dev_list_open(libusb_context  *ctx,
                                           uint8_t          bus_num,
                                           uint8_t          dev_addr,
                                           uint16_t         vid,
                                           uint16_t         pid,
                                           uhd_dev        **plist);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __UHD_DEV_LIST_H__ */

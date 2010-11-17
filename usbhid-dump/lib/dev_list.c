/** @file
 * @brief usbhid-dump - device list
 *
 * Copyright (C) 2010 Nikolai Kondrashov
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

#include "uhd/dev_list.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

bool
uhd_dev_list_valid(const uhd_dev *list)
{
    UHD_DEV_LIST_FOR_EACH(list, list)
        if (!uhd_dev_valid(list))
            return false;

    return true;
}


size_t
uhd_dev_list_len(const uhd_dev *list)
{
    size_t  len = 0;

    UHD_DEV_LIST_FOR_EACH(list, list)
        len++;

    return len;
}


void
uhd_dev_list_close(uhd_dev *list)
{
    uhd_dev *next;

    for (; list != NULL; list = next)
    {
        next = list->next;
        uhd_dev_close(list);
    }
}


enum libusb_error
uhd_dev_list_open(libusb_context *ctx,
                  uint8_t bus_num, uint8_t dev_addr,
                  uint16_t vendor, uint16_t product,
                  uhd_dev **plist)
{
    enum libusb_error                   err         = LIBUSB_ERROR_OTHER;
    libusb_device                     **lusb_list   = NULL;
    ssize_t                             num;
    ssize_t                             idx;
    libusb_device                      *lusb_dev;
    struct libusb_device_descriptor     desc;
    uhd_dev                            *list        = NULL;
    uhd_dev                            *dev;

    assert(ctx != NULL);

    /* Retrieve libusb device list */
    num = libusb_get_device_list(ctx, &lusb_list);
    if (num == LIBUSB_ERROR_NO_MEM)
    {
        err = num;
        goto cleanup;
    }

    /* Find and open the devices */
    for (idx = 0; idx < num; idx++)
    {
        lusb_dev = lusb_list[idx];

        /* Skip devices not matching bus_num/dev_addr mask */
        if ((bus_num != 0 && libusb_get_bus_number(lusb_dev) != bus_num) ||
            (dev_addr != 0 && libusb_get_device_address(lusb_dev) != dev_addr))
            continue;

        /* Skip devices not matching vendor/product mask */
        if (vendor != 0 || product != 0)
        {
            err = libusb_get_device_descriptor(lusb_dev, &desc);
            if (err != LIBUSB_SUCCESS)
                goto cleanup;

            if ((vendor != 0 && vendor != desc.idVendor) ||
                (product != 0 && product != desc.idProduct))
                continue;
        }

        /* Open and append the device to the list */
        err = uhd_dev_open(lusb_dev, &dev);
        if (err != LIBUSB_SUCCESS)
            goto cleanup;

        dev->next = list;
        list = dev;
    }

    /* Free the libusb device list freeing unused devices */
    libusb_free_device_list(lusb_list, true);
    lusb_list = NULL;

    /* Output device list, if requested */
    assert(uhd_dev_list_valid(list));
    if (plist != NULL)
    {
        *plist = list;
        list = NULL;
    }

    /* Done! */
    err = LIBUSB_SUCCESS;

cleanup:

    /* Close the device list if not output */
    uhd_dev_list_close(list);

    /* Free the libusb device list along with devices */
    if (lusb_list != NULL)
        libusb_free_device_list(lusb_list, true);

    return err;
}



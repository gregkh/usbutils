/** @file
 * @brief usbhid-dump - libusb API extensions
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

#include "usbhid_dump/libusb.h"
#include <stdbool.h>

const char *
libusb_strerror(enum libusb_error err)
{
    switch (err)
    {
        case LIBUSB_SUCCESS:
            return "Success";
#define MAP(_name, _desc) \
    case LIBUSB_ERROR_##_name:          \
        return _desc " (ERROR_" #_name ")"
	    MAP(IO,
            "Input/output error");
	    MAP(INVALID_PARAM,
            "Invalid parameter");
	    MAP(ACCESS,
            "Access denied (insufficient permissions)");
	    MAP(NO_DEVICE,
            "No such device (it may have been disconnected)");
	    MAP(NOT_FOUND,
            "Entity not found");
	    MAP(BUSY,
            "Resource busy");
	    MAP(TIMEOUT,
            "Operation timed out");
	    MAP(OVERFLOW,
            "Overflow");
	    MAP(PIPE,
            "Pipe error");
	    MAP(INTERRUPTED,
            "System call interrupted (perhaps due to signal)");
	    MAP(NO_MEM,
            "Insufficient memory");
	    MAP(NOT_SUPPORTED,
            "Operation not supported or unimplemented on this platform");
	    MAP(OTHER, "Other error");
#undef MAP
        default:
            return "Unknown error code";
    }
}


enum libusb_error
libusb_open_device_with_bus_dev(libusb_context         *ctx,
                                uint8_t                 bus_num,
                                uint8_t                 dev_addr,
                                libusb_device_handle  **phandle)
{
    enum libusb_error       err     = LIBUSB_ERROR_OTHER;
    libusb_device         **list    = NULL;
    ssize_t                 num;
    ssize_t                 idx;
    libusb_device          *dev;
    libusb_device_handle   *handle  = NULL;

    /* Retrieve device list */
    num = libusb_get_device_list(ctx, &list);
    if (num == LIBUSB_ERROR_NO_MEM)
    {
        err = num;
        goto cleanup;
    }

    /* Find and open the device */
    for (idx = 0; idx < num; idx++)
    {
        dev = list[idx];

        if (libusb_get_bus_number(dev) == bus_num &&
            libusb_get_device_address(dev) == dev_addr)
        {
            err = libusb_open(dev, &handle);
            if (err != LIBUSB_SUCCESS)
                goto cleanup;
            break;
        }
    }

    /* Free the device list freeing unused devices */
    libusb_free_device_list(list, true);
    list = NULL;

    /* Check if the device is found */
    if (handle == NULL)
    {
        err = LIBUSB_ERROR_NO_DEVICE;
        goto cleanup;
    }

    /* Output the device handle */
    if (phandle != NULL)
    {
        *phandle = handle;
        handle = NULL;
    }

    err = LIBUSB_SUCCESS;

cleanup:

    /* Free the device */
    if (handle != NULL)
        libusb_close(handle);

    /* Free device list along with devices */
    if (list != NULL)
        libusb_free_device_list(list, true);

    return err;
}



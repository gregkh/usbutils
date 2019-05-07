/** @file
 * @brief usbhid-dump - device
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

#include "config.h"

#include "uhd/dev.h"
#include <assert.h>
#include <stdlib.h>

bool
uhd_dev_valid(const uhd_dev *dev)
{
    return dev != NULL &&
           dev->handle != NULL;
}


enum libusb_error
uhd_dev_open(libusb_device     *lusb_dev,
             uhd_dev          **pdev)
{
    enum libusb_error   err;
    uhd_dev            *dev;

    assert(lusb_dev != NULL);

    dev = malloc(sizeof(*dev));
    if (dev == NULL)
        return LIBUSB_ERROR_NO_MEM;

    dev->next       = NULL;

    err = libusb_open(lusb_dev, &dev->handle);
    if (err != LIBUSB_SUCCESS)
    {
        free(dev);
        return err;
    }

    assert(uhd_dev_valid(dev));

    if (pdev != NULL)
        *pdev = dev;
    else
    {
        libusb_close(dev->handle);
        free(dev);
    }

    return LIBUSB_SUCCESS;
}


void
uhd_dev_close(uhd_dev *dev)
{
    if (dev == NULL)
        return;

    assert(uhd_dev_valid(dev));

    libusb_close(dev->handle);
    dev->handle = NULL;

    free(dev);
}



/** @file
 * @brief usbhid-dump - interface list
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

#include "uhd/iface_list.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

bool
uhd_iface_list_valid(const uhd_iface *list)
{
    UHD_IFACE_LIST_FOR_EACH(list, list)
        if (!uhd_iface_valid(list))
            return false;

    return true;
}


size_t
uhd_iface_list_len(const uhd_iface *list)
{
    size_t  len = 0;

    UHD_IFACE_LIST_FOR_EACH(list, list)
        len++;

    return len;
}


void
uhd_iface_list_free(uhd_iface *list)
{
    uhd_iface *next;

    for (; list != NULL; list = next)
    {
        next = list->next;
        uhd_iface_free(list);
    }
}


enum libusb_error
uhd_iface_list_new_from_dev(libusb_device_handle   *handle,
                            uhd_iface             **plist)
{
    enum libusb_error                           err = LIBUSB_ERROR_OTHER;

    struct libusb_config_descriptor            *config          = NULL;
    const struct libusb_interface              *libusb_iface;
    const struct libusb_endpoint_descriptor    *ep_list;
    uint8_t                                     ep_num;
    const struct libusb_endpoint_descriptor    *ep;
    uhd_iface                                  *list            = NULL;
    uhd_iface                                  *last            = NULL;
    uhd_iface                                  *iface;

    assert(handle != NULL);

    /* Retrieve active configuration descriptor */
    err = libusb_get_active_config_descriptor(libusb_get_device(handle),
                                              &config);
    if (err != LIBUSB_SUCCESS)
        goto cleanup;

    /* Build the matching interface list */
    for (libusb_iface = config->interface;
         libusb_iface - config->interface < config->bNumInterfaces;
         libusb_iface++)
    {
        if (libusb_iface->num_altsetting != 1 ||
            libusb_iface->altsetting->bInterfaceClass != LIBUSB_CLASS_HID)
            continue;

        ep_list = libusb_iface->altsetting->endpoint;
        ep_num = libusb_iface->altsetting->bNumEndpoints;

        for (ep = ep_list; (ep - ep_list) < ep_num; ep++)
        {
            if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
                LIBUSB_TRANSFER_TYPE_INTERRUPT ||
                (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) !=
                LIBUSB_ENDPOINT_IN)
                continue;

            iface = uhd_iface_new(
                        handle,
                        libusb_iface->altsetting->bInterfaceNumber,
                        ep->bEndpointAddress, ep->wMaxPacketSize);
            if (iface == NULL)
            {
                err = LIBUSB_ERROR_NO_MEM;
                goto cleanup;
            }

            if (last == NULL)
                list = iface;
            else
                last->next = iface;
            last = iface;

            break;
        }
    }

    /* Output the resulting list, if requested */
    if (plist != NULL)
    {
        *plist = list;
        list = NULL;
    }

cleanup:

    uhd_iface_list_free(list);

    if (config != NULL)
        libusb_free_config_descriptor(config);

    return err;
}


uhd_iface *
uhd_iface_list_fltr_by_num(uhd_iface   *list,
                           int          number)
{
    uhd_iface  *prev;
    uhd_iface  *iface;
    uhd_iface  *next;

    assert(uhd_iface_list_valid(list));
    assert(number < UINT8_MAX);

    if (number < 0)
        return list;

    for (prev = NULL, iface = list; iface != NULL; iface = next)
    {
        next = iface->next;
        if (iface->number == (uint8_t)number)
            prev = iface;
        else
        {
            if (prev == NULL)
                list = next;
            else
                prev->next = next;
            free(iface);
        }
    }

    return list;
}



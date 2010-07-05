/** @file
 * @brief hid-dump - interface
 *
 * Copyright (C) 2010 Nikolai Kondrashov
 *
 * This file is part of hid-dump.
 *
 * Hid-dump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hid-dump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with hid-dump; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @author Nikolai Kondrashov <spbnick@gmail.com>
 *
 * @(#) $Id$
 */

#include "hid_dump/iface.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

hid_dump_iface *
hid_dump_iface_new(uint8_t number)
{
    hid_dump_iface *iface;

    iface = malloc(sizeof(*iface));
    if (iface == NULL)
        return NULL;

    iface->next = NULL;
    iface->number = number;
    iface->detached = false;

    return iface;
}


bool
hid_dump_iface_list_valid(const hid_dump_iface *list)
{
    for (; list != NULL; list = list->next)
        if (!hid_dump_iface_valid(list))
            return false;

    return true;
}


void
hid_dump_iface_list_free(hid_dump_iface *list)
{
    hid_dump_iface *next;

    for (; list != NULL; list = next)
    {
        next = list->next;
        free(list);
    }
}


enum libusb_error
hid_dump_iface_list_new_by_class(libusb_device     *dev,
                                 uint8_t            iface_class,
                                 hid_dump_iface   **plist)
{
    enum libusb_error                   err             = LIBUSB_ERROR_OTHER;
    struct libusb_config_descriptor    *config          = NULL;
    const struct libusb_interface      *libusb_iface;
    hid_dump_iface                     *list            = NULL;
    hid_dump_iface                     *last            = NULL;
    hid_dump_iface                     *iface;

    assert(dev != NULL);

    /* Retrieve active configuration descriptor */
    err = libusb_get_active_config_descriptor(dev, &config);
    if (err != LIBUSB_SUCCESS)
        goto cleanup;

    /* Build the matching interface list */
    for (libusb_iface = config->interface;
         libusb_iface - config->interface < config->bNumInterfaces;
         libusb_iface++)
        if (libusb_iface->num_altsetting == 1 &&
            libusb_iface->altsetting->bInterfaceClass == iface_class)
        {
            fprintf(stderr, "%d\n",
                    libusb_iface->altsetting->bInterfaceNumber);

            iface = hid_dump_iface_new(libusb_iface->altsetting->bInterfaceNumber);
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
        }

    /* Output the resulting list, if requested */
    if (plist != NULL)
    {
        *plist = list;
        list = NULL;
    }

cleanup:

    hid_dump_iface_list_free(list);

    if (config != NULL)
        libusb_free_config_descriptor(config);

    return err;
}


hid_dump_iface *
hid_dump_iface_list_fltr_by_num(hid_dump_iface *list,
                                int             number)
{
    hid_dump_iface *prev;
    hid_dump_iface *iface;
    hid_dump_iface *next;

    assert(hid_dump_iface_list_valid(list));
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

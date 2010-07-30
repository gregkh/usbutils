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

bool
hid_dump_iface_valid(const hid_dump_iface *iface)
{
    return iface != NULL &&
           iface->handle != NULL &&
           iface->number < UINT8_MAX;
}

hid_dump_iface *
hid_dump_iface_new(libusb_device_handle    *handle,
                   uint8_t                  number,
                   uint8_t                  int_in_ep_addr,
                   uint16_t                 int_in_ep_maxp)
{
    hid_dump_iface *iface;

    iface = malloc(sizeof(*iface));
    if (iface == NULL)
        return NULL;

    iface->next             = NULL;
    iface->handle           = handle;
    iface->number           = number;
    iface->int_in_ep_addr   = int_in_ep_addr;
    iface->int_in_ep_maxp   = int_in_ep_maxp;
    iface->detached         = false;
    iface->claimed          = false;
    iface->submitted        = false;

    return iface;
}


void
hid_dump_iface_free(hid_dump_iface *iface)
{
    if (iface == NULL)
        return;

    assert(hid_dump_iface_valid(iface));

    free(iface);
}


bool
hid_dump_iface_list_valid(const hid_dump_iface *list)
{
    for (; list != NULL; list = list->next)
        if (!hid_dump_iface_valid(list))
            return false;

    return true;
}


size_t
hid_dump_iface_list_len(const hid_dump_iface *list)
{
    size_t  len = 0;

    for (; list != NULL; list = list->next)
        len++;

    return len;
}


void
hid_dump_iface_list_free(hid_dump_iface *list)
{
    hid_dump_iface *next;

    for (; list != NULL; list = next)
    {
        next = list->next;
        hid_dump_iface_free(list);
    }
}


enum libusb_error
hid_dump_iface_list_new_from_dev(libusb_device_handle  *handle,
                                 hid_dump_iface       **plist)
{
    enum libusb_error                           err = LIBUSB_ERROR_OTHER;

    struct libusb_config_descriptor            *config          = NULL;
    const struct libusb_interface              *libusb_iface;
    const struct libusb_endpoint_descriptor    *ep_list;
    uint8_t                                     ep_num;
    const struct libusb_endpoint_descriptor    *ep;
    hid_dump_iface                             *list            = NULL;
    hid_dump_iface                             *last            = NULL;
    hid_dump_iface                             *iface;

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

            iface = hid_dump_iface_new(
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


enum libusb_error
hid_dump_iface_list_detach(hid_dump_iface *list)
{
    enum libusb_error   err;

    assert(hid_dump_iface_list_valid(list));

    for (; list != NULL; list = list->next)
    {
        err = libusb_detach_kernel_driver(list->handle, list->number);
        if (err == LIBUSB_SUCCESS)
            list->detached = true;
        else if (err != LIBUSB_ERROR_NOT_FOUND)
            return err;
    }

    return LIBUSB_SUCCESS;
}


enum libusb_error
hid_dump_iface_list_attach(hid_dump_iface *list)
{
    enum libusb_error   err;

    assert(hid_dump_iface_list_valid(list));

    for (; list != NULL; list = list->next)
        if (list->detached)
        {
            err = libusb_attach_kernel_driver(list->handle, list->number);
            if (err != LIBUSB_SUCCESS)
                return err;
            list->detached = false;
        }

    return LIBUSB_SUCCESS;
}


enum libusb_error
hid_dump_iface_list_claim(hid_dump_iface *list)
{
    enum libusb_error   err;

    assert(hid_dump_iface_list_valid(list));

    for (; list != NULL; list = list->next)
    {
        err = libusb_claim_interface(list->handle, list->number);
        if (err != LIBUSB_SUCCESS)
            return err;

        list->claimed = true;
    }

    return LIBUSB_SUCCESS;
}


enum libusb_error
hid_dump_iface_list_clear_halt(hid_dump_iface *list)
{
    enum libusb_error   err;

    assert(hid_dump_iface_list_valid(list));

    for (; list != NULL; list = list->next)
    {
        err = libusb_clear_halt(list->handle, list->int_in_ep_addr);
        if (err != LIBUSB_SUCCESS)
            return err;
    }

    return LIBUSB_SUCCESS;
}


enum libusb_error
hid_dump_iface_list_set_idle(const hid_dump_iface  *list,
                             uint8_t                duration,
                             unsigned int           timeout)
{
    int rc;

    assert(hid_dump_iface_list_valid(list));

    for (; list != NULL; list = list->next)
    {
        rc = libusb_control_transfer(list->handle,
                                     /* host->device, class, interface */
                                     0x21,
                                     /* Set_Idle */
                                     0x0A,
                                     /* duration for all report IDs */
                                     duration << 8,
                                     /* interface */
                                     list->number,
                                     NULL, 0,
                                     timeout);
        /*
         * Ignoring EPIPE, which means a STALL handshake, which is OK on
         * control pipes and indicates request is not supported.
         * See USB 2.0 spec, 8.4.5 Handshake Packets
         */
        if (rc < 0 && rc != LIBUSB_ERROR_PIPE)
            return rc;
    }

    return LIBUSB_SUCCESS;
}


enum libusb_error
hid_dump_iface_list_set_protocol(const hid_dump_iface  *list,
                                 bool                   report,
                                 unsigned int           timeout)
{
    int rc;

    assert(hid_dump_iface_list_valid(list));

    for (; list != NULL; list = list->next)
    {
        rc = libusb_control_transfer(list->handle,
                                     /* host->device, class, interface */
                                     0x21,
                                     /* Set_Protocol */
                                     0x0B,
                                     /* 0 - boot, 1 - report */
                                     report ? 1 : 0,
                                     /* interface */
                                     list->number,
                                     NULL, 0,
                                     timeout);
        /*
         * Ignoring EPIPE, which means a STALL handshake, which is OK on
         * control pipes and indicates request is not supported.
         * See USB 2.0 spec, 8.4.5 Handshake Packets
         */
        if (rc < 0 && rc != LIBUSB_ERROR_PIPE)
            return rc;
    }

    return LIBUSB_SUCCESS;
}


enum libusb_error
hid_dump_iface_list_release(hid_dump_iface *list)
{
    enum libusb_error   err;

    assert(hid_dump_iface_list_valid(list));

    for (; list != NULL; list = list->next)
        if (list->claimed)
        {
            err = libusb_release_interface(list->handle, list->number);
            if (err != LIBUSB_SUCCESS)
                return err;
            list->claimed = false;
        }

    return LIBUSB_SUCCESS;
}



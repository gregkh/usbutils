/** @file
 * @brief usbhid-dump - interface
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

#include "uhd/iface.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

bool
uhd_iface_valid(const uhd_iface *iface)
{
    return iface != NULL &&
           iface->handle != NULL &&
           iface->number < UINT8_MAX;
}

uhd_iface *
uhd_iface_new(libusb_device_handle *handle,
              uint8_t               number,
              uint8_t               int_in_ep_addr,
              uint16_t              int_in_ep_maxp)
{
    uhd_iface  *iface;

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
uhd_iface_free(uhd_iface *iface)
{
    if (iface == NULL)
        return;

    assert(uhd_iface_valid(iface));

    free(iface);
}


enum libusb_error
uhd_iface_detach(uhd_iface *iface)
{
    enum libusb_error   err;

    assert(uhd_iface_valid(iface));

    err = libusb_detach_kernel_driver(iface->handle, iface->number);
    if (err == LIBUSB_SUCCESS)
        iface->detached = true;
    else if (err != LIBUSB_ERROR_NOT_FOUND)
        return err;

    return LIBUSB_SUCCESS;
}


enum libusb_error
uhd_iface_attach(uhd_iface *iface)
{
    enum libusb_error   err;

    assert(uhd_iface_valid(iface));

    if (iface->detached)
    {
        err = libusb_attach_kernel_driver(iface->handle, iface->number);
        if (err != LIBUSB_SUCCESS)
            return err;
        iface->detached = false;
    }

    return LIBUSB_SUCCESS;
}


enum libusb_error
uhd_iface_claim(uhd_iface *iface)
{
    enum libusb_error   err;

    assert(uhd_iface_valid(iface));

    err = libusb_claim_interface(iface->handle, iface->number);
    if (err != LIBUSB_SUCCESS)
        return err;

    iface->claimed = true;

    return LIBUSB_SUCCESS;
}


enum libusb_error
uhd_iface_release(uhd_iface *iface)
{
    enum libusb_error   err;

    assert(uhd_iface_valid(iface));

    err = libusb_release_interface(iface->handle, iface->number);
    if (err != LIBUSB_SUCCESS)
        return err;

    iface->claimed = false;

    return LIBUSB_SUCCESS;
}


enum libusb_error
uhd_iface_clear_halt(uhd_iface *iface)
{
    enum libusb_error   err;

    assert(uhd_iface_valid(iface));

    err = libusb_clear_halt(iface->handle, iface->int_in_ep_addr);
    if (err != LIBUSB_SUCCESS)
        return err;

    return LIBUSB_SUCCESS;
}


enum libusb_error
uhd_iface_set_idle(const uhd_iface    *iface,
                   uint8_t             duration,
                   unsigned int        timeout)
{
    int rc;

    assert(uhd_iface_valid(iface));

    rc = libusb_control_transfer(iface->handle,
                                 /* host->device, class, interface */
                                 0x21,
                                 /* Set_Idle */
                                 0x0A,
                                 /* duration for all report IDs */
                                 duration << 8,
                                 /* interface */
                                 iface->number,
                                 NULL, 0,
                                 timeout);
    /*
     * Ignoring EPIPE, which means a STALL handshake, which is OK on
     * control pipes and indicates request is not supported.
     * See USB 2.0 spec, 8.4.5 Handshake Packets
     */
    if (rc < 0 && rc != LIBUSB_ERROR_PIPE)
        return rc;

    return LIBUSB_SUCCESS;
}


enum libusb_error
uhd_iface_set_protocol(const uhd_iface    *iface,
                       bool                report,
                       unsigned int        timeout)
{
    int rc;

    assert(uhd_iface_valid(iface));

    rc = libusb_control_transfer(iface->handle,
                                 /* host->device, class, interface */
                                 0x21,
                                 /* Set_Protocol */
                                 0x0B,
                                 /* 0 - boot, 1 - report */
                                 report ? 1 : 0,
                                 /* interface */
                                 iface->number,
                                 NULL, 0,
                                 timeout);
    /*
     * Ignoring EPIPE, which means a STALL handshake, which is OK on
     * control pipes and indicates request is not supported.
     * See USB 2.0 spec, 8.4.5 Handshake Packets
     */
    if (rc < 0 && rc != LIBUSB_ERROR_PIPE)
        return rc;

    return LIBUSB_SUCCESS;
}



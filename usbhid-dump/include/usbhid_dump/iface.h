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

#ifndef __USBHID_DUMP_IFACE_H__
#define __USBHID_DUMP_IFACE_H__

#include <stdbool.h>
#include <libusb-1.0/libusb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct usbhid_dump_iface usbhid_dump_iface;

struct usbhid_dump_iface {
    usbhid_dump_iface      *next;
    libusb_device_handle   *handle;         /**< Device handle */
    uint8_t                 number;         /**< Interface number */
    uint8_t                 int_in_ep_addr; /**< Interrupt IN EP address */
    uint16_t                int_in_ep_maxp; /**< Interrupt IN EP maximum
                                                 packet size */
    bool                    detached;       /**< True if the interface was
                                                 detached from the kernel
                                                 driver, false otherwise */
    bool                    claimed;        /**< True if the interface was
                                                 claimed */
    /*
     * This is somewhat hackish and doesn't belong here, since theoretically
     * there could be more than one transfer submitted for an interface.
     * However, we don't do it yet. This flag is used to track transfer
     * cancellation during stream dumping.
     */
    bool                    submitted;      /**< True if an asynchronous
                                                 transfer has been submitted
                                                 for the interface */
};

extern bool usbhid_dump_iface_valid(const usbhid_dump_iface *iface);

extern usbhid_dump_iface *usbhid_dump_iface_new(
                                    libusb_device_handle *handle,
                                    uint8_t               number,
                                    uint8_t               int_in_ep_addr,
                                    uint16_t              int_in_ep_maxp);

extern void usbhid_dump_iface_free(usbhid_dump_iface *iface);

extern bool usbhid_dump_iface_list_valid(const usbhid_dump_iface *list);

static inline bool
usbhid_dump_iface_list_empty(const usbhid_dump_iface *list)
{
    return list == NULL;
}

extern size_t usbhid_dump_iface_list_len(const usbhid_dump_iface *list);

extern void usbhid_dump_iface_list_free(usbhid_dump_iface *list);


/**
 * Fetch a list of HID interfaces from a device.
 *
 * @param handle        The device handle to fetch interface list from.
 * @param plist         Location for the resulting list head; could be NULL.
 *
 * @return Libusb error code.
 */
enum libusb_error
usbhid_dump_iface_list_new_from_dev(libusb_device_handle   *handle,
                                    usbhid_dump_iface     **plist);

/**
 * Filter an interface list by an optional interface number, resulting
 * either in an empty, a single-interface, or an unmodified list.
 *
 * @param plist     The original list head.
 * @param number    The interface number to match against, or a negative
 *                  integer meaning there is no restriction.
 *
 * @return The resulting list head
 */
extern usbhid_dump_iface *usbhid_dump_iface_list_fltr_by_num(
                                            usbhid_dump_iface  *list,
                                            int                 number);

/**
 * Detach all interfaces in a list from their kernel drivers (if any).
 *
 * @param list  The list of interfaces to detach.
 *
 * @return Libusb error code.
 */
extern enum libusb_error usbhid_dump_iface_list_detach(
                                                usbhid_dump_iface  *list);

/**
 * Attach all interfaces in a list to their kernel drivers (if were detached
 * before).
 *
 * @param list  The list of interfaces to attach.
 *
 * @return Libusb error code.
 */
extern enum libusb_error usbhid_dump_iface_list_attach(
                                                usbhid_dump_iface  *list);

/**
 * Claim all interfaces in a list.
 *
 * @param list  The list of interfaces to claim.
 *
 * @return Libusb error code.
 */
extern enum libusb_error usbhid_dump_iface_list_claim(
                                                usbhid_dump_iface  *list);

/**
 * Set idle duration on all interfaces in a list; ignore errors indicating
 * missing support.
 *
 * @param list      The list of interfaces to set idle duration on.
 * @param duration  The duration in 4 ms steps starting from 4 ms.
 * @param timeout   The request timeout, ms.
 *
 * @return Libusb error code.
 */
extern enum libusb_error usbhid_dump_iface_list_set_idle(
                                    const usbhid_dump_iface    *list,
                                    uint8_t                     duration,
                                    unsigned int                timeout);

/**
 * Set HID protocol on all interfaces in a list; ignore errors indicating
 * missing support.
 *
 * @param list      The list of interfaces to set idle duration on.
 * @param report    True for "report" protocol, false for "boot" protocol.
 * @param timeout   The request timeout, ms.
 *
 * @return Libusb error code.
 */
extern enum libusb_error usbhid_dump_iface_list_set_protocol(
                                    const usbhid_dump_iface    *list,
                                    bool                        report,
                                    unsigned int                timeout);

/**
 * Clear halt condition on input interrupt endpoints of all interfaces.
 *
 * @param list  The list of interfaces to clear halt condition on.
 *
 * @return Libusb error code.
 */
extern enum libusb_error usbhid_dump_iface_list_clear_halt(
                                                usbhid_dump_iface  *list);
/**
 * Release all interfaces in a list (if were claimed before).
 *
 * @param list  The list of interfaces to release.
 *
 * @return Libusb error code.
 */
extern enum libusb_error usbhid_dump_iface_list_release(
                                            usbhid_dump_iface  *list);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __USBHID_DUMP_IFACE_H__ */

/** @file
 * @brief usbhid-dump - miscellaneous declarations
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

#ifndef __UHD_MISC_H__
#define __UHD_MISC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

/** HID extra descriptor record */
typedef struct uhd_hid_descriptor_extra uhd_hid_descriptor_extra;

struct uhd_hid_descriptor_extra {
    uint8_t     bDescriptorType;
    uint16_t    wDescriptorLength;
};

/** HID class-specific descriptor */
typedef struct uhd_hid_descriptor uhd_hid_descriptor;

struct uhd_hid_descriptor {
    uint8_t                     bLength;
    uint8_t                     bDescriptorType;
    uint16_t                    bcdHID;
    uint8_t                     bCountryCode;
    uint8_t                     bNumDescriptors;
    uhd_hid_descriptor_extra    extra[1];
};

#pragma pack()

/**
 * Maximum descriptor size.
 *
 * @note 4096 here is maximum control buffer length.
 */
#define UHD_MAX_DESCRIPTOR_SIZE 4096

/** Generic USB I/O timeout, ms */
#define UHD_IO_TIMEOUT      1000

/** Wildcard bus number */
#define UHD_BUS_NUM_ANY     0

/** Wildcard device address */
#define UHD_DEV_ADDR_ANY    0

/** Wildcard vendor ID */
#define UHD_VID_ANY         0

/** Wildcard product ID */
#define UHD_PID_ANY         0

/** Wildcard interface number */
#define UHD_IFACE_NUM_ANY   UINT8_MAX

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __UHD_MISC_H__ */

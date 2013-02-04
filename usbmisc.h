/*****************************************************************************/
/*
 *      usbmisc.h  --  Misc USB routines
 *
 *      Copyright (C) 2003  Aurelien Jarno (aurelien@aurel32.net)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

/*****************************************************************************/

#ifndef _USBMISC_H
#define _USBMISC_H

#include <libusb.h>

/* ---------------------------------------------------------------------- */

extern libusb_device *get_usb_device(libusb_context *ctx, const char *path);

extern char *get_dev_string(libusb_device_handle *dev, u_int8_t id);

/* ---------------------------------------------------------------------- */
#endif /* _USBMISC_H */

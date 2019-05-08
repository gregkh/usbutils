// SPDX-License-Identifier: GPL-2.0+
/*
 * usbhid-dump - libusb API extensions
 *
 * Copyright (C) 2010 Nikolai Kondrashov <spbnick@gmail.com>
 */

#include "config.h"

#include "uhd/libusb.h"
#include <stdbool.h>


#ifndef HAVE_LIBUSB_STRERROR
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
#endif

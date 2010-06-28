/** @file
 * @brief hid-dump - entry point
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

#include <assert.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>

static bool
usage(FILE *stream, const char *progname)
{
    return
        fprintf(
            stream,
            "Usage: %s [OPTIONS]... <entity> <bus> <dev> [if]\n"
            "Dump a USB device HID report descriptor or stream."
            "\n"
            "Arguments:\n"
            "  entity   \"descriptor\", \"stream\" or \"both\";\n"
            "           can be abbreviated\n"
            "  bus      bus number\n"
            "  dev      device number\n"
            "  if       interface number; if ommitted,\n"
            "           all the device HID interfaces are dumped\n"
            "\n"
            "Options:\n"
            "  -h, --help   this help message\n"
            "\n",
            progname) >= 0;
}


static const char *
libusb_strerror(enum libusb_error err)
{
    switch (err)
    {
        case LIBUSB_SUCCESS:
            return "Success";
#define MAP(_name, _desc) \
    case LIBUSB_ERROR_##_name:          \
        return _desc " (" #_name ")"
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


static bool
strisblank(const char *str)
{
    for (; *str != '\0'; str++)
        if (!isblank(*str))
            return false;
    return true;
}

#define ERROR_CLEANUP(_fmt, _args...) \
    do {                                                    \
        fprintf(stderr, "Failed to " _fmt "\n", ##_args);   \
        goto cleanup;                                       \
    } while (0)

#define LIBUSB_ERROR_CLEANUP(_fmt, _args...) \
    do {                                        \
        fprintf(stderr,                         \
                "Failed to " _fmt ": %s\n",     \
                ##_args, libusb_strerror(err)); \
        goto cleanup;                           \
    } while (0)


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

    /* Find the device */
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

    /* Output the handle */
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


ssize_t
libusb_find_interfaces_by_class(libusb_device          *dev,
                                uint8_t                 if_class,
                                int                   **pif_list,
                                size_t                 *pif_num)
{
    enum libusb_error                   err     = LIBUSB_ERROR_OTHER;
    struct libusb_config_descriptor    *config  = NULL;
    const struct libusb_interface      *iface;
    uint8_t                             i;

    (void)if_class;
    (void)pif_list;
    (void)pif_num;

    err = libusb_get_active_config_descriptor(dev, &config);
    if (err != LIBUSB_SUCCESS)
        goto cleanup;

    for (iface = config->interface, i = 0;
         i < config->bNumInterfaces;
         i++, iface++)
    {
        fprintf(stderr, "if index: %hhu, alt settings: %d\n",
                i, iface->num_altsetting);
    }
    

cleanup:

    if (config != NULL)
        libusb_free_config_descriptor(config);

    return err;
}

#if 0
static int
run_handle(bool                     dump_descriptor,
           bool                     dump_stream,
           libusb_device_handle    *handle,
           int                      if_num)
{
}
#endif


static int
run(bool    dump_descriptor,
    bool    dump_stream, 
    uint8_t bus_num,
    uint8_t dev_addr,
    int     if_num)
{
    int                     result  = 1;
    enum libusb_error       err;
    libusb_context         *ctx     = NULL;
    libusb_device         **list    = NULL;
    libusb_device_handle   *handle  = NULL;

    /* Initialize libusb context */
    err = libusb_init(&ctx);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_ERROR_CLEANUP("create libusb context");

    /* Set libusb debug level */
    libusb_set_debug(ctx, 3);

    /* Find and open the device */
    err = libusb_open_device_with_bus_dev(ctx, bus_num, dev_addr, &handle);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_ERROR_CLEANUP("find and open the device");

#if 0
    /* Run with the handle */
    result = run_handle(dump_descriptor, dump_stream, handle, if_num);
#else
    (void)dump_descriptor;
    (void)dump_stream;
    (void)if_num;
#endif

cleanup:

    /* Free the device */
    if (handle != NULL)
        libusb_close(handle);

    /* Free device list along with devices */
    if (list != NULL)
        libusb_free_device_list(list, true);

    /* Cleanup libusb context, if necessary */
    if (ctx != NULL)
        libusb_exit(ctx);

    return result;
}


typedef enum opt_val {
    OPT_VAL_HELP           = 'h',
} opt_val;


int
main(int argc, char **argv)
{
    static const struct option long_opt_list[] = {
        {.val       = OPT_VAL_HELP,
         .name      = "help",
         .has_arg   = no_argument,
         .flag      = NULL},
        {.val       = 0,
         .name      = NULL,
         .has_arg   = 0,
         .flag      = NULL}
    };

    static const char  *short_opt_list = "h";

    char        c;

    const char *entity;
    const char *bus_str;
    const char *dev_str;
    const char *if_str      = "";

    const char    **arg_list[] = {&entity, &bus_str, &dev_str, &if_str};
    const size_t    req_arg_num = 3;
    const size_t    max_arg_num = 4;
    size_t          i;
    char           *end;

    bool            dump_descriptor = false;
    bool            dump_stream     = false;
    long            bus_num;
    long            dev_num;
    long            if_num          = -1;

    /*
     * Parse command line arguments
     */
    while ((c = getopt_long(argc, argv,
                            short_opt_list, long_opt_list, NULL)) >= 0)
    {
        switch (c)
        {
            case OPT_VAL_HELP:
                usage(stdout, program_invocation_short_name);
                return 0;
                break;
            case '?':
                usage(stderr, program_invocation_short_name);
                return 1;
                break;
        }
    }

#define USAGE_ERROR(_fmt, _args...) \
    do {                                                \
        fprintf(stderr, _fmt "\n", ##_args);            \
        usage(stderr, program_invocation_short_name);   \
        return 1;                                       \
    } while (0)

    /*
     * Assign positional parameters
     */
    for (i = 0; i < max_arg_num && optind < argc; i++, optind++)
        *arg_list[i] = argv[optind];

    if (i < req_arg_num)
        USAGE_ERROR("Not enough arguments");

    /*
     * Parse and verify positional parameters
     */
    if (strncmp(entity, "descriptor", strlen(entity)) == 0)
        dump_descriptor = true;
    else if (strncmp(entity, "stream", strlen(entity)) == 0)
        dump_stream = true;
    else if (strncmp(entity, "both", strlen(entity)) == 0)
    {
        dump_descriptor = true;
        dump_stream = true;
    }
    else
        USAGE_ERROR("Unknown entity \"%s\"", entity);

    errno = 0;
    bus_num = strtol(bus_str, &end, 0);
    if (errno != 0 || !strisblank(end) || bus_num <= 0 || bus_num > 255)
        USAGE_ERROR("Invalid bus number \"%s\"", bus_str);

    errno = 0;
    dev_num = strtol(dev_str, &end, 0);
    if (errno != 0 || !strisblank(end) || dev_num <= 0 || dev_num > 255)
        USAGE_ERROR("Invalid device number \"%s\"", dev_str);

    if (!strisblank(if_str))
    {
        errno = 0;
        if_num = strtol(if_str, &end, 0);
        if (errno != 0 || !strisblank(end) || if_num < 0 || if_num >= 255)
            USAGE_ERROR("Invalid interface number \"%s\"", if_str);
    }

    return run(dump_descriptor, dump_stream, bus_num, dev_num, if_num);
}



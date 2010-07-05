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

#include "hid_dump/iface.h"
#include "hid_dump/str.h"
#include "hid_dump/libusb.h"

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
            "Usage: %s [OPTION]... <bus> <dev> [if]\n"
            "Dump a USB device HID report descriptor and/or stream."
            "\n"
            "Arguments:\n"
            "  bus      bus number\n"
            "  dev      device number\n"
            "  if       interface number; if ommitted,\n"
            "           all device HID interfaces are dumped\n"
            "\n"
            "Options:\n"
            "  -h, --help           this help message\n"
            "  -e, --entity=STRING  what to dump: either \"descriptor\",\n"
            "                       \"stream\" or \"both\"; value can be\n"
            "                       abbreviated\n"
            "\n"
            "Default options: --entity=descriptor\n"
            "\n",
            progname) >= 0;
}


#define ERROR(_fmt, _args...) \
    fprintf(stderr, _fmt "\n", ##_args)

#define FAILURE(_fmt, _args...) \
    fprintf(stderr, "Failed to " _fmt "\n", ##_args)

#define LIBUSB_FAILURE(_fmt, _args...) \
    ERROR_CLEANUP("Failed to " _fmt ": %s", ##_args, libusb_strerror(err))

#define ERROR_CLEANUP(_fmt, _args...) \
    do {                                \
        ERROR(_fmt, ##_args);           \
        goto cleanup;                   \
    } while (0)

#define LIBUSB_ERROR_CLEANUP(_fmt, _args...) \
    ERROR_CLEANUP(_fmt ": %s", ##_args, libusb_strerror(err))

#define FAILURE_CLEANUP(_fmt, _args...) \
    ERROR_CLEANUP("Failed to " _fmt, ##_args)

#define LIBUSB_FAILURE_CLEANUP(_fmt, _args...) \
    LIBUSB_ERROR_CLEANUP("Failed to " _fmt, ##_args)


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
    int     iface_num)
{
    int                     result      = 1;
    enum libusb_error       err;
    libusb_context         *ctx         = NULL;
    libusb_device         **list        = NULL;
    libusb_device_handle   *handle      = NULL;
    hid_dump_iface         *iface_list  = NULL;

    /* Initialize libusb context */
    err = libusb_init(&ctx);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("create libusb context");

    /* Set libusb debug level */
    libusb_set_debug(ctx, 3);

    /* Find and open the device */
    err = libusb_open_device_with_bus_dev(ctx, bus_num, dev_addr, &handle);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("find and open the device");

    /* Retrieve the list of HID interfaces */
    err = hid_dump_iface_list_new_by_class(handle, 3, &iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("find a HID interface");

    /* Filter the interface list by specified interface number */
    iface_list = hid_dump_iface_list_fltr_by_num(iface_list, iface_num);
    if (hid_dump_iface_list_empty(iface_list))
        ERROR_CLEANUP("No matching HID interfaces");

    /* Detach interfaces */
    err = hid_dump_iface_list_detach(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("detach interface(s) from "
                               "the kernel drivers");
#if 0
    /* Run with the handle */
    result = run_handle(dump_descriptor, dump_stream, handle, if_num);
#else
    (void)dump_descriptor;
    (void)dump_stream;
    (void)iface_num;
    result = 0;
#endif

cleanup:

    /* Attach interfaces back */
    err = hid_dump_iface_list_attach(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE("attach interface(s) to the kernel drivers");

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
    OPT_VAL_HELP    = 'h',
    OPT_VAL_ENTITY  = 'e',
} opt_val;


int
main(int argc, char **argv)
{
    static const struct option long_opt_list[] = {
        {.val       = OPT_VAL_HELP,
         .name      = "help",
         .has_arg   = no_argument,
         .flag      = NULL},
        {.val       = OPT_VAL_ENTITY,
         .name      = "entity",
         .has_arg   = required_argument,
         .flag      = NULL},
        {.val       = 0,
         .name      = NULL,
         .has_arg   = 0,
         .flag      = NULL}
    };

    static const char  *short_opt_list = "he:";

    char            c;
    const char     *bus_str;
    const char     *dev_str;
    const char     *if_str      = "";

    const char    **arg_list[] = {&bus_str, &dev_str, &if_str};
    const size_t    req_arg_num = 2;
    const size_t    max_arg_num = 3;
    size_t          i;
    char           *end;

    bool            dump_descriptor = true;
    bool            dump_stream     = false;
    long            bus_num;
    long            dev_num;
    long            if_num          = -1;

#define USAGE_ERROR(_fmt, _args...) \
    do {                                                \
        fprintf(stderr, _fmt "\n", ##_args);            \
        usage(stderr, program_invocation_short_name);   \
        return 1;                                       \
    } while (0)

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
            case OPT_VAL_ENTITY:
                if (strncmp(optarg, "descriptor", strlen(optarg)) == 0)
                {
                    dump_descriptor = true;
                    dump_stream = false;
                }
                else if (strncmp(optarg, "stream", strlen(optarg)) == 0)
                {
                    dump_descriptor = false;
                    dump_stream = true;
                }
                else if (strncmp(optarg, "both", strlen(optarg)) == 0)
                {
                    dump_descriptor = true;
                    dump_stream = true;
                }
                else
                    USAGE_ERROR("Unknown entity \"%s\"", optarg);

                break;
            case '?':
                usage(stderr, program_invocation_short_name);
                return 1;
                break;
        }
    }

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
    errno = 0;
    bus_num = strtol(bus_str, &end, 0);
    if (errno != 0 || !hid_dump_strisblank(end) ||
        bus_num <= 0 || bus_num > 255)
        USAGE_ERROR("Invalid bus number \"%s\"", bus_str);

    errno = 0;
    dev_num = strtol(dev_str, &end, 0);
    if (errno != 0 || !hid_dump_strisblank(end) ||
        dev_num <= 0 || dev_num > 255)
        USAGE_ERROR("Invalid device number \"%s\"", dev_str);

    if (!hid_dump_strisblank(if_str))
    {
        errno = 0;
        if_num = strtol(if_str, &end, 0);
        if (errno != 0 || !hid_dump_strisblank(end) ||
            if_num < 0 || if_num >= 255)
            USAGE_ERROR("Invalid interface number \"%s\"", if_str);
    }

    return run(dump_descriptor, dump_stream, bus_num, dev_num, if_num);
}



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
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>

/**
 * Maximum descriptor size.
 *
 * 4096 here is maximum control buffer length.
 */
#define MAX_DESCRIPTOR_SIZE 4096

/**
 * USB I/O timeout
 */
#define TIMEOUT 1000

#define ERROR(_fmt, _args...) \
    fprintf(stderr, _fmt "\n", ##_args)

#define FAILURE(_fmt, _args...) \
    fprintf(stderr, "Failed to " _fmt "\n", ##_args)

#define LIBUSB_FAILURE(_fmt, _args...) \
    FAILURE(_fmt ": %s", ##_args, libusb_strerror(err))

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

/**< Number of the signal causing the exit */
static volatile sig_atomic_t exit_signum  = 0;

static void
exit_sighandler(int signum)
{
    if (exit_signum == 0)
        exit_signum = signum;
}

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


static void
dump(uint8_t        iface_num,
     const char    *entity,
     const uint8_t *ptr,
     size_t         len)
{
    static const char   xd[]    = "0123456789ABCDEF";
    static char         buf[]   = " XX\n";
    size_t              pos;
    uint8_t             b;

    fprintf(stdout, "%.3hhu:%s\n", iface_num, entity);

    for (pos = 1; len > 0; len--, ptr++, pos++)
    {
        b = *ptr;
        buf[1] = xd[b >> 4];
        buf[2] = xd[b & 0xF];

        fwrite(buf, ((pos % 16 == 0) ? 4 : 3), 1, stdout);
    }

    if (pos % 16 != 1)
        fputc('\n', stdout);
    fputc('\n', stdout);
}


static bool
dump_iface_list_descriptor(const hid_dump_iface *list)
{
    const hid_dump_iface   *iface;
    uint8_t                 buf[MAX_DESCRIPTOR_SIZE];
    int                     rc;
    enum libusb_error       err;

    for (iface = list; iface != NULL; iface = iface->next)
    {
        rc = libusb_control_transfer(iface->handle,
                                     /*
                                      * FIXME Don't really know why it
                                      * should be the EP 1, and why EP 0
                                      * doesn't work sometimes.
                                      */
                                     LIBUSB_ENDPOINT_IN + 1,
                                     LIBUSB_REQUEST_GET_DESCRIPTOR,
                                     (LIBUSB_DT_REPORT << 8), iface->number,
                                     buf, sizeof(buf), TIMEOUT);
        if (rc < 0)
        {
            err = rc;
            LIBUSB_FAILURE("retrieve interface #%hhu "
                           "report descriptor", iface->number);
            return false;
        }
        dump(iface->number, "DESCRIPTOR", buf, rc);
    }

    return true;
}


static void
dump_iface_list_stream_cb(struct libusb_transfer *transfer)
{
    enum libusb_error       err;
    const hid_dump_iface   *iface;

    assert(transfer != NULL);

    iface = (hid_dump_iface *)transfer->user_data;
    assert(hid_dump_iface_valid(iface));

    switch (transfer->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            /* Dump the result */
            dump(iface->number, "STREAM",
                 transfer->buffer, transfer->actual_length);
            /* Resubmit the transfer */
            err = libusb_submit_transfer(transfer);
            if (err != LIBUSB_SUCCESS)
                LIBUSB_FAILURE("resubmit a transfer");
            break;
#define MAP(_name) \
    case LIBUSB_TRANSFER_##_name:                               \
        fprintf(stderr, "%.3hhu:%s\n", iface->number, #_name);  \
        break

        MAP(ERROR);
        MAP(TIMED_OUT);
        MAP(CANCELLED);
        MAP(STALL);
        MAP(NO_DEVICE);
        MAP(OVERFLOW);

#undef MAP
        default:
            break;
    }
}


static bool
dump_iface_list_stream(libusb_context *ctx, const hid_dump_iface *list)
{
    bool                        result              = false;
    enum libusb_error           err;
    size_t                      transfer_num        = 0;
    struct libusb_transfer    **transfer_list       = NULL;
    struct libusb_transfer    **ptransfer;
    const hid_dump_iface       *iface;

    /* Calculate number of interfaces and thus transfers */
    transfer_num = hid_dump_iface_list_len(list);

    /* Allocate transfer list */
    transfer_list = malloc(sizeof(*transfer_list) * transfer_num);
    if (transfer_list == NULL)
        FAILURE_CLEANUP("allocate transfer list");

    /* Zero transfer list */
    for (ptransfer = transfer_list;
         (size_t)(ptransfer - transfer_list) < transfer_num;
         ptransfer++)
        *ptransfer = NULL;

    /* Allocate transfers */
    for (ptransfer = transfer_list;
         (size_t)(ptransfer - transfer_list) < transfer_num;
         ptransfer++)
    {
        *ptransfer = libusb_alloc_transfer(0);
        if (*ptransfer == NULL)
            FAILURE_CLEANUP("allocate a transfer");
    }

    /* Initialize the transfers as interrupt transfers */
    for (ptransfer = transfer_list, iface = list;
         (size_t)(ptransfer - transfer_list) < transfer_num;
         ptransfer++, iface = iface->next)
    {
        void           *buf;
        const size_t    len = iface->int_in_ep_maxp;

        /* Allocate the transfer buffer */
        buf = malloc(len);   
        if (len > 0 && buf == NULL)
            FAILURE_CLEANUP("allocate a transfer buffer");

        /* Initialize the transfer */
        libusb_fill_interrupt_transfer(*ptransfer,
                                       iface->handle, iface->int_in_ep_addr,
                                       buf, len,
                                       dump_iface_list_stream_cb,
                                       (void *)iface,
                                       0);

        /* Ask to free the buffer when the transfer is freed */
        (*ptransfer)->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
    }

    /* Submit first transfer requests */
    for (ptransfer = transfer_list;
         (size_t)(ptransfer - transfer_list) < transfer_num;
         ptransfer++)
    {
        err = libusb_submit_transfer(*ptransfer);
        if (err != LIBUSB_SUCCESS)
            LIBUSB_ERROR_CLEANUP("submit a transfer");
    }

    /* Run the event machine */
    while (exit_signum == 0)
    {
        err = libusb_handle_events(ctx);
        if (err != LIBUSB_SUCCESS && err != LIBUSB_ERROR_INTERRUPTED)
            LIBUSB_FAILURE_CLEANUP("handle transfer events");
    }

    result = true;

cleanup:

    /* TODO Cancel the transfers */

    /* Free transfer list along with transfers and their buffers */
    if (transfer_list != NULL)
    {
        for (ptransfer = transfer_list;
             (size_t)(ptransfer - transfer_list) < transfer_num;
             ptransfer++)
            libusb_free_transfer(*ptransfer);

        free(transfer_list);
    }

    return result;
}


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

    /* Retrieve the list of HID interfaces from a device */
    err = hid_dump_iface_list_new_from_dev(handle, &iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("find a HID interface");

    /* Filter the interface list by specified interface number */
    iface_list = hid_dump_iface_list_fltr_by_num(iface_list, iface_num);
    if (hid_dump_iface_list_empty(iface_list))
        ERROR_CLEANUP("No matching HID interfaces");

    /* Detach interfaces */
    err = hid_dump_iface_list_detach(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("detach the interface(s) from "
                               "the kernel drivers");

    /* Claim interfaces */
    err = hid_dump_iface_list_claim(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("claim the interface(s)");

    /* Run with the prepared interface list */
    result = (!dump_descriptor || dump_iface_list_descriptor(iface_list)) &&
             (!dump_stream || dump_iface_list_stream(ctx, iface_list))
               ? 0
               : 1;

cleanup:

    /* Release the interfaces back */
    err = hid_dump_iface_list_release(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE("release the interface(s)");

    /* Attach interfaces back */
    err = hid_dump_iface_list_attach(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE("attach the interface(s) to the kernel drivers");

    /* Free the interface list */
    hid_dump_iface_list_free(iface_list);

    /* Free the device */
    if (handle != NULL)
        libusb_close(handle);

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

    int             result;

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

    struct sigaction    sa;

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

    /*
     * Setup signal handlers
     */
    /* Setup SIGINT to terminate gracefully */
    sigaction(SIGINT, NULL, &sa);
    if (sa.sa_handler != SIG_IGN)
    {
        sa.sa_handler = exit_sighandler;
        sigemptyset(&sa.sa_mask);
        sigaddset(&sa.sa_mask, SIGTERM);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
    }

    /* Setup SIGTERM to terminate gracefully */
    sigaction(SIGTERM, NULL, &sa);
    if (sa.sa_handler != SIG_IGN)
    {
        sa.sa_handler = exit_sighandler;
        sigemptyset(&sa.sa_mask);
        sigaddset(&sa.sa_mask, SIGINT);
        sa.sa_flags = 0;
        sigaction(SIGTERM, &sa, NULL);
    }

    result = run(dump_descriptor, dump_stream, bus_num, dev_num, if_num);

    /*
     * Restore signal handlers
     */
    sigaction(SIGINT, NULL, &sa);
    if (sa.sa_handler != SIG_IGN)
        signal(SIGINT, SIG_DFL);

    sigaction(SIGTERM, NULL, &sa);
    if (sa.sa_handler != SIG_IGN)
        signal(SIGTERM, SIG_DFL);

    /*
     * Reproduce the signal used to stop the program to get proper exit
     * status.
     */
    if (exit_signum != 0)
        raise(exit_signum);

    return result;
}



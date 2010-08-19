/** @file
 * @brief usbhid-dump - entry point
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

#include "usbhid_dump/iface.h"
#include "usbhid_dump/str.h"
#include "usbhid_dump/libusb.h"

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

/**< "Stream paused" flag - non-zero if paused */
static volatile sig_atomic_t stream_paused = 0;

static void
stream_pause_sighandler(int signum)
{
    (void)signum;
    stream_paused = 1;
}

static void
stream_resume_sighandler(int signum)
{
    (void)signum;
    stream_paused = 0;
}

/**< "Stream feedback" flag - non-zero if feedback is enabled */
static volatile sig_atomic_t stream_feedback = 0;

static bool
usage(FILE *stream, const char *progname)
{
    return
        fprintf(
            stream,
"Usage: %s [OPTION]... <bus> <dev> [if]\n"
"Dump a USB device HID report descriptor(s) and/or stream(s)."
"\n"
"Arguments:\n"
"  bus                      bus number (1-255)\n"
"  dev                      device address (1-255)\n"
"  if                       interface number (0-254);\n"
"                           if omitted, all device HID interfaces\n"
"                           are dumped\n"
"\n"
"Options:\n"
"  -h, --help               this help message\n"
"  -e, --entity=STRING      what to dump: either \"descriptor\",\n"
"                           \"stream\" or \"both\"; value can be\n"
"                           abbreviated\n"
"  -p, --stream-paused      start with the stream dump output paused\n"
"  -f, --stream-feedback    enable stream dumping feedback: for every\n"
"                           transfer dumped a dot is printed to stderr\n"
"\n"
"Default options: --entity=descriptor\n"
"\n"
"Signals:\n"
"  USR1/USR2                pause/resume the stream dump output\n"
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
    struct timeval      tv;

    gettimeofday(&tv, NULL);

    fprintf(stdout, "%.3hhu:%-16s %20llu.%.6u\n",
            iface_num, entity,
            (unsigned long long int)tv.tv_sec,
            (unsigned int)tv.tv_usec);

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

    fflush(stdout);
}


static bool
dump_iface_list_descriptor(const usbhid_dump_iface *list)
{
    const usbhid_dump_iface    *iface;
    uint8_t                     buf[MAX_DESCRIPTOR_SIZE];
    int                         rc;
    enum libusb_error           err;

    for (iface = list; iface != NULL; iface = iface->next)
    {
        rc = libusb_control_transfer(iface->handle,
                                     /* See HID spec, 7.1.1 */
                                     0x81,
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
    enum libusb_error   err;
    usbhid_dump_iface  *iface;

    assert(transfer != NULL);

    iface = (usbhid_dump_iface *)transfer->user_data;
    assert(usbhid_dump_iface_valid(iface));

    /* Clear interface "has transfer submitted" flag */
    iface->submitted = false;

    switch (transfer->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            /* Dump the result */
            if (!stream_paused)
            {
                dump(iface->number, "STREAM",
                     transfer->buffer, transfer->actual_length);
                if (stream_feedback)
                    fputc('.', stderr);
            }
            /* Resubmit the transfer */
            err = libusb_submit_transfer(transfer);
            if (err != LIBUSB_SUCCESS)
                LIBUSB_FAILURE("resubmit a transfer");
            /* Set interface "has transfer submitted" flag */
            iface->submitted = true;
            break;
#define MAP(_name) \
    case LIBUSB_TRANSFER_##_name:                               \
        fprintf(stderr, "%.3hhu:%s\n", iface->number, #_name);  \
        break

        MAP(ERROR);
        MAP(TIMED_OUT);
        MAP(STALL);
        MAP(NO_DEVICE);
        MAP(OVERFLOW);

#undef MAP
        default:
            break;
    }
}


static bool
dump_iface_list_stream(libusb_context *ctx, usbhid_dump_iface *list)
{
    bool                        result              = false;
    enum libusb_error           err;
    size_t                      transfer_num        = 0;
    struct libusb_transfer    **transfer_list       = NULL;
    struct libusb_transfer    **ptransfer;
    usbhid_dump_iface          *iface;
    bool                        submitted           = false;

    /* Set report protocol on all interfaces */
    err = usbhid_dump_iface_list_set_protocol(list, true, TIMEOUT);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_ERROR_CLEANUP("set report protocol");

    /* Set infinite idle duration on all interfaces */
    err = usbhid_dump_iface_list_set_idle(list, 0, TIMEOUT);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_ERROR_CLEANUP("set infinite idle duration");

    /* Calculate number of interfaces and thus transfers */
    transfer_num = usbhid_dump_iface_list_len(list);

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
        /*
         * Set user_data to NULL explicitly, since libusb_alloc_transfer
         * does memset to zero only and zero is not NULL, strictly speaking.
         */
        (*ptransfer)->user_data = NULL;
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
                                       /* Infinite timeout */
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
        /* Set interface "has transfer submitted" flag */
        ((usbhid_dump_iface *)(*ptransfer)->user_data)->submitted = true;
        /* Set "have any submitted transfers" flag */
        submitted = true;
    }

    /* Run the event machine */
    while (submitted && exit_signum == 0)
    {
        /* Handle the transfer events */
        err = libusb_handle_events(ctx);
        if (err != LIBUSB_SUCCESS && err != LIBUSB_ERROR_INTERRUPTED)
            LIBUSB_FAILURE_CLEANUP("handle transfer events");

        /* Check if there are any submitted transfers left */
        submitted = false;
        for (ptransfer = transfer_list;
             (size_t)(ptransfer - transfer_list) < transfer_num;
             ptransfer++)
        {
            iface = (usbhid_dump_iface *)(*ptransfer)->user_data;

            if (iface != NULL && iface->submitted)
                submitted = true;
        }
    }

    /* If all the transfers were terminated unexpectedly */
    if (transfer_num > 0 && !submitted)
        ERROR_CLEANUP("No more interfaces to dump");

    result = true;

cleanup:

    /* Cancel the transfers */
    if (submitted)
    {
        submitted = false;
        for (ptransfer = transfer_list;
             (size_t)(ptransfer - transfer_list) < transfer_num;
             ptransfer++)
        {
            iface = (usbhid_dump_iface *)(*ptransfer)->user_data;

            if (iface != NULL && iface->submitted)
            {
                err = libusb_cancel_transfer(*ptransfer);
                if (err == LIBUSB_SUCCESS)
                    submitted = true;
                else
                {
                    LIBUSB_FAILURE("cancel a transfer, ignoring");
                    /*
                     * XXX are we really sure
                     * the transfer won't be finished?
                     */
                    iface->submitted = false;
                }
            }
        }
    }

    /* Wait for transfer cancellation */
    while (submitted)
    {
        /* Handle cancellation events */
        err = libusb_handle_events(ctx);
        if (err != LIBUSB_SUCCESS && err != LIBUSB_ERROR_INTERRUPTED)
        {
            LIBUSB_FAILURE("handle transfer cancellation events, "
                           "aborting transfer cancellation");
            break;
        }

        /* Check if there are any submitted transfers left */
        submitted = false;
        for (ptransfer = transfer_list;
             (size_t)(ptransfer - transfer_list) < transfer_num;
             ptransfer++)
        {
            iface = (usbhid_dump_iface *)(*ptransfer)->user_data;

            if (iface != NULL && iface->submitted)
                submitted = true;
        }
    }

    /*
     * Free transfer list along with non-submitted transfers and their
     * buffers.
     */
    if (transfer_list != NULL)
    {
        for (ptransfer = transfer_list;
             (size_t)(ptransfer - transfer_list) < transfer_num;
             ptransfer++)
        {
            iface = (usbhid_dump_iface *)(*ptransfer)->user_data;

            /*
             * Only free a transfer if it is not submitted. Better leak some
             * memory than have some important memory overwritten.
             */
            if (iface == NULL || !iface->submitted)
                libusb_free_transfer(*ptransfer);
        }

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
    usbhid_dump_iface      *iface_list  = NULL;

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
    err = usbhid_dump_iface_list_new_from_dev(handle, &iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("find a HID interface");

    /* Filter the interface list by specified interface number */
    iface_list = usbhid_dump_iface_list_fltr_by_num(iface_list, iface_num);
    if (usbhid_dump_iface_list_empty(iface_list))
        ERROR_CLEANUP("No matching HID interfaces");

    /* Detach interfaces */
    err = usbhid_dump_iface_list_detach(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("detach the interface(s) from "
                               "the kernel drivers");

    /* Claim interfaces */
    err = usbhid_dump_iface_list_claim(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE_CLEANUP("claim the interface(s)");

    /* Run with the prepared interface list */
    result = (!dump_descriptor || dump_iface_list_descriptor(iface_list)) &&
             (!dump_stream || dump_iface_list_stream(ctx, iface_list))
               ? 0
               : 1;

cleanup:

    /* Release the interfaces back */
    err = usbhid_dump_iface_list_release(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE("release the interface(s)");

    /* Attach interfaces back */
    err = usbhid_dump_iface_list_attach(iface_list);
    if (err != LIBUSB_SUCCESS)
        LIBUSB_FAILURE("attach the interface(s) to the kernel drivers");

    /* Free the interface list */
    usbhid_dump_iface_list_free(iface_list);

    /* Free the device */
    if (handle != NULL)
        libusb_close(handle);

    /* Cleanup libusb context, if necessary */
    if (ctx != NULL)
        libusb_exit(ctx);

    return result;
}


typedef enum opt_val {
    OPT_VAL_HELP            = 'h',
    OPT_VAL_ENTITY          = 'e',
    OPT_VAL_STREAM_PAUSED   = 'p',
    OPT_VAL_STREAM_FEEDBACK = 'f',
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
        {.val       = OPT_VAL_STREAM_PAUSED,
         .name      = "stream-paused",
         .has_arg   = no_argument,
         .flag      = NULL},
        {.val       = OPT_VAL_STREAM_FEEDBACK,
         .name      = "stream-feedback",
         .has_arg   = no_argument,
         .flag      = NULL},
        {.val       = 0,
         .name      = NULL,
         .has_arg   = 0,
         .flag      = NULL}
    };

    static const char  *short_opt_list = "he:pf";

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
            case OPT_VAL_STREAM_PAUSED:
                stream_paused = 1;
                break;
            case OPT_VAL_STREAM_FEEDBACK:
                stream_feedback = 1;
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
    if (errno != 0 || !usbhid_dump_strisblank(end) ||
        bus_num <= 0 || bus_num > 255)
        USAGE_ERROR("Invalid bus number \"%s\"", bus_str);

    errno = 0;
    dev_num = strtol(dev_str, &end, 0);
    if (errno != 0 || !usbhid_dump_strisblank(end) ||
        dev_num <= 0 || dev_num > 255)
        USAGE_ERROR("Invalid device address \"%s\"", dev_str);

    if (!usbhid_dump_strisblank(if_str))
    {
        errno = 0;
        if_num = strtol(if_str, &end, 0);
        if (errno != 0 || !usbhid_dump_strisblank(end) ||
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
        sa.sa_flags = 0;    /* NOTE: no SA_RESTART on purpose */
        sigaction(SIGINT, &sa, NULL);
    }

    /* Setup SIGTERM to terminate gracefully */
    sigaction(SIGTERM, NULL, &sa);
    if (sa.sa_handler != SIG_IGN)
    {
        sa.sa_handler = exit_sighandler;
        sigemptyset(&sa.sa_mask);
        sigaddset(&sa.sa_mask, SIGINT);
        sa.sa_flags = 0;    /* NOTE: no SA_RESTART on purpose */
        sigaction(SIGTERM, &sa, NULL);
    }

    /* Setup SIGUSR1/SIGUSR2 to pause/resume the stream output */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = stream_pause_sighandler;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = stream_resume_sighandler;
    sigaction(SIGUSR2, &sa, NULL);

    /* Make stdout buffered - we will flush it explicitly */
    setbuf(stdout, NULL);

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



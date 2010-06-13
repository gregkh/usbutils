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
            "  entity   \"descriptor\" or \"stream\", can be abbreviated\n"
            "  bus      bus number\n"
            "  dev      device number\n"
            "  if       interface number; can be ommitted,\n"
            "           if the device has only one HID interface\n"
            "\n"
            "Options:\n"
            "  -h, --help   this help message\n",
            "\n"
            progname) >= 0;
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

    const char *entity;
    const char *bus_str;
    const char *dev_str;
    const char *if_str;

    const char *arg_list[] = {&entity, &bus_str, &dev_str, &if_str, NULL};
    size_t      i;

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

    /*
     * Assign positional parameters
     */
    for (i = 0;
         i < sizeof(arg_list) / sizeof(*arg_list) && optind < argc;
         i++, optind++)
        arg_list = argv[optind];

    if (*parg != NULL

    if (optind < argc)
    {
        entity = argv[optind++];
        if (optind < argc)
        {
            bus_str = argv[optind++];
            if (optind < argc)
            {
                dev_str = argv[optind++];
                if (optind < argc)
                {
                    if_str = argv[optind++];
                    if (optind < argc)
                    {
                        fprintf(stderr, "Too many arguments\n");
                        usage(stderr, program_invocation_short_name);
                        return 1;
                    }
                }
            }
        }
    }

}

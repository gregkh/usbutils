/*****************************************************************************/

/*
 *      usmodules.c  --  pcimodules like utility for the USB bus
 *     
 *	lsusb.c is derived from:
 *
 *	lspci.c					by Thomas Sailer,
 *	pcimodules.c				by Adam J. Richter
 *	linux-2.4.0-test10/include/linux/usb.h	probably by Randy Dunlap
 *
 *	The code in usbmodules not derived from elsewhere was written by
 *	Adam J. Richter.  David Brownell added the --mapfile and --version
 *	options. Aurelien Jarno modified the code to use libusb.
 *
 *	Copyright (C) 2000, 2001  Yggdrasil Computing, Inc.
 *      Copyright (C) 1999  Thomas Sailer (sailer@ife.ee.ethz.ch)
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 */

/*****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <usb.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "usbmodules.h"
#include "usbmisc.h"

#define _GNU_SOURCE
#include <getopt.h>

#define OPT_STRING "c:d:hi:m:p:t:v"
static struct option long_options[] = {
	{"check",	required_argument,	NULL,	'c'},
	{"device",      required_argument,      NULL,   'd'},
	{"help",	no_argument, 		NULL,	'h'},
	{"interface",	required_argument,	NULL,	'i'},
	{"mapfile",     required_argument,      NULL,   'm'},
	{"product",	required_argument,	NULL,	'p'},
	{"type",	required_argument,	NULL,	't'},
	{"version",	no_argument, 		NULL,	'v'},
	{ 0,            0,                      NULL,   0}
};

#define MODDIR "/lib/modules"
#define USBMAP "modules.usbmap"

#define LINELENGTH     8000 

static char *checkname = NULL;
struct usbmap_entry *usbmap_list;

static void *
xmalloc(unsigned int size) {
       void *result = malloc(size);
       if (result == NULL) {
               fprintf(stderr, "Memory allocation failure.\n");
               exit(1);
       }
       return result;
}

static int
scan_without_flags(const char *line, struct usbmap_entry *entry, char *name) {
	unsigned int driver_info;
	if (sscanf(line,
		   "%s 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		   name,
		   &entry->idVendor,
		   &entry->idProduct,
		   &entry->bcdDevice_lo,
		   &entry->bcdDevice_hi,
		   &entry->bDeviceClass,
		   &entry->bDeviceSubClass,
		   &entry->bDeviceProtocol,
		   &entry->bInterfaceClass,
		   &entry->bInterfaceSubClass,
		   &entry->bInterfaceProtocol,
		   &driver_info) != 12)
		return 0;

	entry->match_flags = 0;

	/* idVendor==0 is the wildcard for both idVendor and idProduct,
	   because idProduct==0 is a legitimate product ID. */
	if (entry->idVendor)
		entry->match_flags |= USB_MATCH_VENDOR | USB_MATCH_PRODUCT;

	if (entry->bcdDevice_lo)
		entry->match_flags |= USB_MATCH_DEV_LO;

	if (entry->bcdDevice_hi)
		entry->match_flags |= USB_MATCH_DEV_HI;

	if (entry->bDeviceClass)
		entry->match_flags |= USB_MATCH_DEV_CLASS;

	if (entry->bDeviceSubClass)
		entry->match_flags |= USB_MATCH_DEV_SUBCLASS;

	if (entry->bDeviceProtocol)
		entry->match_flags |= USB_MATCH_DEV_PROTOCOL;

	if (entry->bInterfaceClass)
		entry->match_flags |= USB_MATCH_INT_CLASS;

	if (entry->bInterfaceSubClass)
		entry->match_flags |= USB_MATCH_INT_SUBCLASS;

	if (entry->bInterfaceProtocol)
		entry->match_flags |= USB_MATCH_INT_PROTOCOL;

	return 1;
}

static int
scan_with_flags(const char *line, struct usbmap_entry *entry, char *name) {
	unsigned int driver_info;
	return (sscanf(line, "%s 0x%x 0x%x "
		       "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
		       name,
		       &entry->match_flags,
		       &entry->idVendor,
		       &entry->idProduct,
		       &entry->bcdDevice_lo,
		       &entry->bcdDevice_hi,
		       &entry->bDeviceClass,
		       &entry->bDeviceSubClass,
		       &entry->bDeviceProtocol,
		       &entry->bInterfaceClass,
		       &entry->bInterfaceSubClass,
		       &entry->bInterfaceProtocol,
		       &driver_info) == 13);
}

void
read_modules_usbmap(char *pathname)
{
       char filename[MAXPATHLEN];
       FILE *usbmap_file;
       char line[LINELENGTH];
       struct usbmap_entry *prev;
       struct usbmap_entry *entry;
       char name[LINELENGTH];

       if (pathname == NULL) {
	       struct utsname utsname;
	       if (uname(&utsname) < 0) {
		       perror("uname");
		       exit(1);
	       }
	       sprintf(filename, "%s/%s/%s", MODDIR, utsname.release, USBMAP);
	       pathname = filename;
       }
       if ((usbmap_file = fopen(pathname, "r")) == NULL) {
               perror(pathname);
               exit(1);
       }

       prev = NULL;
       while(fgets(line, LINELENGTH, usbmap_file) != NULL) {
               if (line[0] == '#')
                       continue;

	       if (line[0] == '\n')
		       continue;

               entry = xmalloc(sizeof(struct usbmap_entry));

	       if (!scan_with_flags(line, entry, name) &&
		   !scan_without_flags(line, entry, name)) {
                       fprintf (stderr,
                               "modules.usbmap unparsable line: %s.\n", line);
                       free(entry);
                       continue;
               }

               /* Optimize memory allocation a bit, in case someday we
                  have Linux systems with ~100,000 modules.  It also
                  allows us to just compare pointers to avoid trying
                  to load a module twice. */
               if (prev == NULL || strcmp(name, prev->name) != 0) {
                       entry->name = xmalloc(strlen(name)+1);
                       strcpy(entry->name, name);
                       entry->selected_ptr = &entry->selected;
                       entry->selected = 0;
                       prev = entry;
               } else {
                       entry->name = prev->name;
                       entry->selected_ptr = prev->selected_ptr;
               }
               entry->next = usbmap_list;
               usbmap_list = entry;
       }
       fclose(usbmap_file);
}

/* Match modules is called once per interface.  We know that
   each device has at least one interface, because, according
   to the USB 2.0 Specification, section 9.6.3, "A USB device has
   one or more configuration descriptors.  Each configuration has
   one or more interfaces and each interface has zero or more endpoints."
   So, there must be at least one interface on a device.
*/

static void
match_modules(struct usb_device_descriptor *device_descriptor, 
              struct usb_interface_descriptor *interface_descriptor)
{
        struct usbmap_entry *mod;

        for (mod = usbmap_list; mod != NULL; mod = mod->next) {

                if ((mod->match_flags & USB_MATCH_VENDOR) &&
                    mod->idVendor != device_descriptor->idVendor)
                        continue;

                if ((mod->match_flags & USB_MATCH_PRODUCT) &&
                    mod->idProduct != device_descriptor->idProduct)
                        continue;

                if ((mod->match_flags & USB_MATCH_DEV_LO) &&
                    mod->bcdDevice_lo > device_descriptor->bcdDevice)
                        continue;

                if ((mod->match_flags & USB_MATCH_DEV_HI) &&
                    mod->bcdDevice_hi < device_descriptor->bcdDevice)
                        continue;

                if ((mod->match_flags & USB_MATCH_DEV_CLASS) &&
                    mod->bDeviceClass != device_descriptor->bDeviceClass)
                        continue;

                if ((mod->match_flags & USB_MATCH_DEV_SUBCLASS) &&
                    mod->bDeviceSubClass != device_descriptor->bDeviceSubClass)
                        continue;

                if ((mod->match_flags & USB_MATCH_DEV_PROTOCOL) &&
                    mod->bDeviceProtocol != device_descriptor->bDeviceProtocol)
                        continue;

                if ((mod->match_flags & USB_MATCH_INT_CLASS) &&
                    mod->bInterfaceClass != interface_descriptor->bInterfaceClass)
                        continue;

                if ((mod->match_flags & USB_MATCH_INT_SUBCLASS) &&
                    mod->bInterfaceSubClass != interface_descriptor->bInterfaceSubClass)
                        continue;

                if ((mod->match_flags & USB_MATCH_INT_PROTOCOL) &&
                    mod->bInterfaceProtocol != interface_descriptor->bInterfaceProtocol)
                        continue;

		if (checkname != NULL) {
			if (strcmp(checkname, mod->name) == 0)
				exit(0);  /* Program returns "success" */
		} else if (!(*mod->selected_ptr)) {
                        *(mod->selected_ptr) = 1;
                        printf ("%s\n", mod->name);
                }
        }
}

static void process_device(const char *path)
{
	struct usb_device *dev;
	struct usb_dev_handle *udev;
	int i, j, k;

	dev = get_usb_device(path);

	if (!dev) {
		fprintf(stderr, "Cannot open %s\n", path);	       
		return;
	}
		
			
	udev = usb_open(dev);
	
	for (i = 0 ; i < dev->descriptor.bNumConfigurations ; i++)
		for (j = 0 ; j < dev->config[i].bNumInterfaces ; j++) 
		       	for (k = 0 ; k < dev->config[i].interface[j].num_altsetting ; k++)
			        match_modules(&dev->descriptor, 
					      &dev->config[i].interface[j].altsetting[k]);

	usb_close(udev);
}

static void process_args(char *product,
			 char *type,
			 char *interface)
{
	int a, b, c;
	struct usb_device_descriptor dd;
	struct usb_interface_descriptor id;

	memset(&dd, 0, sizeof(dd));
	memset(&id, 0, sizeof(id));
	if (product == NULL ||
	    sscanf(product, "%hx/%hx/%hx", &dd.idVendor, &dd.idProduct, &dd.bcdDevice) != 3) {
		fprintf(stderr, "Bad product format: '%s'\n", product);
		return;
	}
	if (type == NULL || sscanf(type, "%d/%d/%d", &a, &b, &c) != 3) {
		fprintf(stderr, "Bad type format: '%s'", type);
		return;
	}
	dd.bDeviceClass = a;
	dd.bDeviceSubClass = b;
	dd.bDeviceProtocol = c;
	if (dd.bDeviceClass == 0) {
		/* interface must be specified for device class 0 */
		if (interface == NULL ||
		    sscanf(interface, "%d/%d/%d", &a, &b, &c) != 3) {
			fprintf(stderr, "Bad interface format: '%s'\n", interface);
			return;
		}
		id.bInterfaceClass = a;
		id.bInterfaceSubClass = b;
		id.bInterfaceProtocol = c;
	} else {
		/* interface maybe given. if so, check and use arg */
		if (interface != NULL && *interface != '\0' &&
		    sscanf(interface, "%d/%d/%d", &a, &b, &c) != 3) {
			fprintf(stderr, "Bad interface format: '%s'\n", interface);
			return;
		}
		id.bInterfaceClass = a;
		id.bInterfaceSubClass = b;
		id.bInterfaceProtocol = c;
	}
    	match_modules(&dd, &id);
}

int main (int argc, char *argv[])
{
       int opt_index = 0;
       int opt;
       char *device = NULL;
       char *pathname = NULL;
       char *product = NULL, *type = NULL, *interface = NULL;

       while ((opt = getopt_long(argc, argv, OPT_STRING, long_options, &opt_index)) != -1) {
               switch(opt) {
	       case 'c':
		       checkname = optarg;
		       break;
	       case 'd':
		       device = optarg;
		       break;
	       case 'h':
		       printf ("Usage: usbmodules [options]...\n"
			       "Lists kernel modules corresponding to USB devices currently plugged\n"
			       "\n"
			       "OPTIONS\n"
			       "  -d, --device /proc/bus/usb/NNN/NNN\n"
			       "      Selects which device usbmodules will examine\n"
			       "  -c, --check module\n"
			       "      Check if the given module's exported USB ID patterns matches\n"
			       "  -m, --mapfile /etc/hotplug/usb.handmap\n"
			       "      Specify a mapfile\n"
			       "  -p, --product xx/xx/xx\n"
			       "  -t, --type dd/dd/dd\n"
			       "  -i, --interface dd/dd/dd\n"
			       "  -h, --help\n"
			       "      Print help screen\n"
			       "  -v, --version\n"
			       "      Show version of program\n"
			       "\n");
		       return 0;
	       case 'm':
		       pathname = optarg;
		       break;
	       case 'i':
		       interface = optarg;
		       break;
	       case 'p':
		       product = optarg;
		       break;
	       case 't':
		       type = optarg;
		       break;
	       case 'v':
		       puts (VERSION);
		       return 0;
	       default:
		       fprintf(stderr,
			       "Unknown argument character \"%c\".\n",
			       opt);
		       return 1;
               }
       }       

       if (device == NULL &&
	   (product == NULL || type == NULL || interface == NULL) ) {
               fprintf (stderr,
                        "You must specify a device with something like:\n"
                        "\tusbmodules --device /proc/bus/usb/001/009\n"
			"or\n"
			"\tusbmodules --product 82d/100/100 --type 0/0/0 --interface 0/0/0\n");
              return 1;
       }
       
       read_modules_usbmap(pathname);
       usb_init();
       usb_find_busses();
       usb_find_devices();
       if (device != NULL)
	       process_device(device);
       if (product != NULL && type != NULL)
	       process_args(product, type, interface);

	if (checkname != NULL)
		return 1; /* The module being checked was not needed */

       return 0;
}

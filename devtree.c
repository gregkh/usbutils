/*****************************************************************************/

/*
 *      devtree.c  --  USB device tree.
 *
 *      Copyright (C) 1999 Thomas Sailer, sailer@ife.ee.ethz.ch
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
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

/*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "devtree.h"

/* ---------------------------------------------------------------------- */

LIST_HEAD(usbbuslist);

/* ---------------------------------------------------------------------- */

static void freedev(struct usbdevnode *dev)
{
	free(dev);
}

static void freebus(struct usbbusnode *bus)
{
	free(bus);
}

/* ---------------------------------------------------------------------- */

static void markdel(struct list_head *list)
{
	struct usbdevnode *dev;
	struct list_head *list2;

	for (list2 = list->next; list2 != list; list2 = list2->next) {
		dev = list_entry(list2, struct usbdevnode, list);
		dev->flags |= USBFLG_DELETED;
		markdel(&dev->childlist);
	}
}

void devtree_markdeleted(void)
{
	struct usbbusnode *bus;
	struct list_head *list;
	
	for(list = usbbuslist.next; list != &usbbuslist; list = list->next) {
		bus = list_entry(list, struct usbbusnode, list);
		markdel(&bus->childlist);
	}
}

struct usbbusnode *devtree_findbus(unsigned int busn)
{
	struct usbbusnode *bus;
	struct list_head *list;
	
	for(list = usbbuslist.next; list != &usbbuslist; list = list->next) {
		bus = list_entry(list, struct usbbusnode, list);
		if (bus->busnum == busn)
			return bus;
	}
	return NULL;
}

static struct usbdevnode *findsubdevice(struct list_head *list, unsigned int devn)
{
	struct usbdevnode *dev, *dev2;
	struct list_head *list2;

	for (list2 = list->next; list2 != list; list2 = list2->next) {
		dev = list_entry(list2, struct usbdevnode, list);
		if (dev->devnum == devn)
			return dev;
		dev2 = findsubdevice(&dev->childlist, devn);
		if (dev2)
			return dev2;
	}
	return NULL;
}

struct usbdevnode *devtree_finddevice(struct usbbusnode *bus, unsigned int devn)
{
	return findsubdevice(&bus->childlist, devn);
}

/* ---------------------------------------------------------------------- */

void devtree_parsedevfile(int fd)
{
	char buf[16384];
	char *start, *end, *lineend, *cp;
	int ret;
	unsigned int devnum = 0, busnum = 0, parentdevnum = 0, level = 0;
	unsigned int class = 0xff, vendor = 0xffff, prodid = 0xffff, speed = 0;
	struct usbbusnode *bus;
	struct usbdevnode *dev, *dev2;

	devtree_markdeleted();
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
		lprintf(0, "lseek: %s (%d)\n", strerror(errno), errno);
	ret = read(fd, buf, sizeof(buf)-1);
	if (ret == -1)
		lprintf(0, "read: %s (%d)\n", strerror(errno), errno);
	end = buf + ret;
	*end = 0;
	start = buf;
	while (start < end) {
		lineend = strchr(start, '\n');
		if (!lineend)
			break;
		*lineend = 0;
		switch (start[0]) {
		case 'T':  /* topology line */
			if ((cp = strstr(start, "Dev#="))) {
				devnum = strtoul(cp + 5, NULL, 0);
			} else
				devnum = 0;
			if ((cp = strstr(start, "Bus="))) {
				busnum = strtoul(cp + 4, NULL, 10);
			} else
				busnum = 0;
			if ((cp = strstr(start, "Prnt="))) {
				parentdevnum = strtoul(cp + 5, NULL, 10);
			} else
				parentdevnum = 0;
			if ((cp = strstr(start, "Lev="))) {
				level = strtoul(cp + 4, NULL, 10);
			} else
				level = 0;
			if (strstr(start, "Spd=1.5"))
				speed = 1;
			else if (strstr(start, "Spd=12"))
				speed = 2;
			else 
				speed = 0;
			break;

		case 'D':
			if ((cp = strstr(start, "Cls="))) {
				class = strtoul(cp + 4, NULL, 16);
			} else
				class = 0xff;
			break;

		case 'P':
			if ((cp = strstr(start, "Vendor="))) {
				vendor = strtoul(cp + 7, NULL, 16);
			} else
				vendor = 0xffff;
			if ((cp = strstr(start, "ProdID="))) {
				prodid = strtoul(cp + 7, NULL, 16);
			} else
				prodid = 0xffff;
			/* print device */
#if 0
			printf("Device %3d Vendor %04x Product ID %04x Class %02x Speed %s\n",
			       devnum, vendor, prodid, class, speed == 2 ? "12 MBPS" : speed == 1 ? "1.5 MBPS" : "unknown");
#endif
			if (!(bus = devtree_findbus(busnum))) {
				if (!(bus = malloc(sizeof(struct usbbusnode))))
					lprintf(0, "Out of memory\n");
				bus->busnum = busnum;
				bus->flags = USBFLG_NEW;
				INIT_LIST_HEAD(&bus->childlist);
				list_add_tail(&bus->list, &usbbuslist);
			} else {
				bus->flags &= ~USBFLG_DELETED;
			}
			if (!(dev = devtree_finddevice(bus, devnum)) || dev->vendorid != vendor || dev->productid != prodid) {
				if (!(dev = malloc(sizeof(struct usbdevnode))))
					lprintf(0, "Out of memory\n");
				dev->devnum = devnum;
				dev->flags = USBFLG_NEW;
				dev->bus = bus;
				dev->vendorid = vendor;
				dev->productid = prodid;
				INIT_LIST_HEAD(&dev->childlist);
				if (level == 0 && parentdevnum == 0) {
					list_add_tail(&dev->list, &bus->childlist);
					dev->parent = NULL;
				} else {
					if (!(dev2 = devtree_finddevice(bus, parentdevnum)))
						lprintf(0, "Bus %d Device %d Parent Device %d not found\n", busnum, devnum, parentdevnum);
					dev->parent = dev2;
					list_add_tail(&dev->list, &dev2->childlist);
				}
			} else {
				dev->flags &= ~USBFLG_DELETED;
			}
			break;

		default:
			break;
		}
#if 0
		printf("line: %s\n", start);
#endif
		start = lineend + 1;
	}
}

/* ---------------------------------------------------------------------- */

static void deletetree(struct list_head *list, unsigned int force)
{
	struct usbdevnode *dev;
	struct list_head *list2;

	for (list2 = list->next; list2 != list;) {
		dev = list_entry(list2, struct usbdevnode, list);
		list2 = list2->next;
		deletetree(&dev->childlist, force || dev->flags & USBFLG_DELETED);
		if (!force && !(dev->flags & USBFLG_DELETED))
			continue;
		list_del(&dev->list);
		INIT_LIST_HEAD(&dev->list);
		devtree_devdisconnect(dev);
		freedev(dev);
	}
}

static void newtree(struct list_head *list)
{
	struct usbdevnode *dev;
	struct list_head *list2;

	for (list2 = list->next; list2 != list; list2 = list2->next) {
		dev = list_entry(list2, struct usbdevnode, list);
		if (dev->flags & USBFLG_NEW)
			devtree_devconnect(dev);
		dev->flags &= ~USBFLG_NEW;
		newtree(&dev->childlist);
	}
}

void devtree_processchanges(void)
{
	struct list_head *list;
	struct usbbusnode *bus;

	for (list = usbbuslist.next; list != &usbbuslist;) {
		bus = list_entry(list, struct usbbusnode, list);
		list = list->next;
		deletetree(&bus->childlist, bus->flags & USBFLG_DELETED);
		if (!(bus->flags & USBFLG_DELETED))
			continue;
		list_del(&bus->list);
		INIT_LIST_HEAD(&bus->list);
		devtree_busdisconnect(bus);
		freebus(bus);
	}
	for (list = usbbuslist.next; list != &usbbuslist; list = list->next) {
		bus = list_entry(list, struct usbbusnode, list);
		if (bus->flags & USBFLG_NEW)
			devtree_busconnect(bus);
		bus->flags &= ~USBFLG_NEW;
		newtree(&bus->childlist);
	}
}

/* ---------------------------------------------------------------------- */

static void dumpdevlist(struct list_head *list, unsigned int level, unsigned int mask)
{
	struct usbdevnode *dev;
	struct list_head *list2;
	char buf[512];
	char *cp;
	unsigned int i;

	for (list2 = list->next; list2 != list; ) {
		dev = list_entry(list2, struct usbdevnode, list);
		list2 = list2->next;
		for (cp = buf, i = 0; i < level; i++) {
			*cp++ = (mask & (1 << i)) ? '|' : ' ';
			*cp++ = ' ';
		}
		if (list2 != list) {
			mask |= (1 << level);
			*cp++ = '|';
		} else {
			mask &= ~(1 << level);
			*cp++ = '`';
		}
		*cp++ = '-';
		snprintf(cp, buf + sizeof(buf) - cp, "Dev# %3d Vendor 0x%04x Product 0x%04x",
			 dev->devnum, dev->vendorid, dev->productid);
		lprintf(1, "%s\n", buf);
		dumpdevlist(&dev->childlist, level+1, mask);
	}
}

void devtree_dump(void)
{
	struct list_head *list;
	struct usbbusnode *bus;

	for (list = usbbuslist.next; list != &usbbuslist; list = list->next) {
		bus = list_entry(list, struct usbbusnode, list);
		lprintf(1, "Bus# %2d\n", bus->busnum);
		dumpdevlist(&bus->childlist, 0, 0);
	}
}

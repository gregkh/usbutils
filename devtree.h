/*****************************************************************************/

/*
 *      devtree.h  --  USB device tree.
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
 */

/*****************************************************************************/

#ifndef _DEVTREE_H
#define _DEVTREE_H

/* ---------------------------------------------------------------------- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "list.h"

/* ---------------------------------------------------------------------- */

#define USBFLG_DELETED    1
#define USBFLG_NEW        2

/* ---------------------------------------------------------------------- */

struct usbbusnode {
	struct list_head list;
	struct list_head childlist;
	unsigned int flags;

	unsigned int busnum;
};

struct usbdevnode {
	struct list_head list;
	struct list_head childlist;
	unsigned int flags;

	struct usbbusnode *bus;
	struct usbdevnode *parent;

	unsigned int devnum;
	unsigned int vendorid;
	unsigned int productid;
};

extern struct list_head usbbuslist;

/* ---------------------------------------------------------------------- */

extern void devtree_markdeleted(void);
extern struct usbbusnode *devtree_findbus(unsigned int busn);
extern struct usbdevnode *devtree_finddevice(struct usbbusnode *bus,
					     unsigned int devn);
extern void devtree_parsedevfile(int fd);
extern void devtree_busconnect(struct usbbusnode *bus);
extern void devtree_busdisconnect(struct usbbusnode *bus);
extern void devtree_devconnect(struct usbdevnode *dev);
extern void devtree_devdisconnect(struct usbdevnode *dev);
extern void devtree_processchanges(void);
extern void devtree_dump(unsigned int verblevel);

extern int lprintf(unsigned int vl, const char *format, ...)
	__attribute__ ((format (printf, 2, 3)));

/* ---------------------------------------------------------------------- */
#endif /* _DEVTREE_H */

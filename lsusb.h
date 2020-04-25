// SPDX-License-Identifier: GPL-2.0+
/* Copyright 2011 (c) Greg Kroah-Hartman <gregkh@suse.de> */
#ifndef _LSUSB_H
#define _LSUSB_H

extern int lsusb_t(void);
extern int lsusb_init_usb_tree(void);
extern char *get_sysfs_name(uint8_t bnum, uint8_t dnum);
extern unsigned int verblevel;

#endif

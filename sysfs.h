// SPDX-License-Identifier: GPL-2.0+
/*
 * Helpers for querying USB properties from sysfs
 */

#ifndef _SYSFS_H
#define _SYSFS_H
/* ---------------------------------------------------------------------- */

extern int read_sysfs_prop(char *buf, size_t size, uint8_t bnum, uint8_t pnum, char *propname);

/* ---------------------------------------------------------------------- */
#endif /* _SYSFS_H */

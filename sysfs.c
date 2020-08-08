// SPDX-License-Identifier: GPL-2.0+
/*
 * Helpers for querying USB properties from sysfs
 */

#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/limits.h>

#include "sysfs.h"

#define SYSFS_DEV_ATTR_PATH "/sys/bus/usb/devices/%d-%d/%s"

int read_sysfs_prop(char *buf, size_t size, uint8_t bnum, uint8_t pnum, char *propname)
{
	int n, fd;
	char path[PATH_MAX];

	buf[0] = '\0';
	snprintf(path, sizeof(path), SYSFS_DEV_ATTR_PATH, bnum, pnum, propname);
	fd = open(path, O_RDONLY);

	if (fd == -1)
		return 0;

	n = read(fd, buf, size);

	if (n > 0)
		buf[n-1] = '\0';  // Turn newline into null terminator

	close(fd);
	return n;
}

// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Misc USB routines
 *
 * Copyright (C) 2003 Aurelien Jarno (aurelien@aurel32.net)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <iconv.h>
#include <langinfo.h>

#include "usbmisc.h"

/* ---------------------------------------------------------------------- */

static const char *devbususb = "/dev/bus/usb";

/* ---------------------------------------------------------------------- */

static int readlink_recursive(const char *path, char *buf, size_t bufsize)
{
	char current[PATH_MAX + 1];
	char target[PATH_MAX + 1];
	char joined[PATH_MAX + 1];
	unsigned int depth;

	if (bufsize == 0)
		return 0;

	snprintf(current, sizeof(current), "%s", path ? path : "");

	for (depth = 0; depth < 32; depth++) {
		size_t dir_len;
		ssize_t ret;
		char *slash;

		ret = readlink(current, target, sizeof(target) - 1);
		if (ret <= 0)
			break;
		target[ret] = '\0';

		if (target[0] == '/') {
			snprintf(current, sizeof(current), "%s", target);
			continue;
		}

		slash = strrchr(current, '/');
		dir_len = slash ? (size_t)(slash - current + 1) : 0;
		if (dir_len >= sizeof(joined))
			break;
		if (dir_len + strlen(target) >= sizeof(joined))
			break;

		snprintf(joined, sizeof(joined), "%.*s%s", (int)dir_len, current, target);
		snprintf(current, sizeof(current), "%s", joined);
	}

	snprintf(buf, bufsize, "%s", current);
	return strlen(buf);
}

static char *get_absolute_path(const char *path, char *result,
			       size_t result_size)
{
	const char *ppath;	/* pointer on the input string */
	char *presult;		/* pointer on the output string */
	char *result_start;

	ppath = path;
	presult = result;
	result_start = result;
	result[0] = 0;

	if (path == NULL)
		return result;

	if (*ppath != '/') {
		char *cwd = getcwd(result, result_size);
		if (cwd == NULL) {
			result_start[0] = 0;
			return result_start;
		}
		presult += strlen(result_start);
		result_size -= strlen(result_start);

		*presult++ = '/';
		result_size--;
	}

	while (*ppath != 0 && result_size > 1) {
		if (*ppath == '/') {
			do
				ppath++;
			while (*ppath == '/');
			*presult++ = '/';
			result_size--;
		} else if (*ppath == '.' && *(ppath + 1) == '.' &&
			   *(ppath + 2) == '/' && *(presult - 1) == '/') {
			if ((presult - 1) != result) {
				/* go one directory upper */
				do {
					presult--;
					result_size++;
				} while (*(presult - 1) != '/');
			}
			ppath += 3;
		} else if (*ppath == '.'  &&
			   *(ppath + 1) == '/' &&
			   *(presult - 1) == '/') {
			ppath += 2;
		} else {
			*presult++ = *ppath++;
			result_size--;
		}
	}
	/* Don't forget to mark the end of the string! */
	*presult = 0;

	return result_start;
}

libusb_device *get_usb_device(libusb_context *ctx, const char *path)
{
	libusb_device **list;
	libusb_device *dev;
	ssize_t num_devs, i;
	char device_path[PATH_MAX + 1];
	char absolute_path[PATH_MAX + 1];

	readlink_recursive(path, device_path, sizeof(device_path));
	get_absolute_path(device_path, absolute_path, sizeof(absolute_path));

	dev = NULL;
	num_devs = libusb_get_device_list(ctx, &list);
	if (num_devs < 0)
		return NULL;

	for (i = 0; i < num_devs; ++i) {
		uint8_t bnum = libusb_get_bus_number(list[i]);
		uint8_t dnum = libusb_get_device_address(list[i]);

		snprintf(device_path, sizeof(device_path), "%s/%03u/%03u",
			 devbususb, bnum, dnum);
		if (!strcmp(device_path, absolute_path)) {
			dev = list[i];
			libusb_ref_device(dev);
			break;
		}
	}

	libusb_free_device_list(list, 1);
	return dev;
}

static char *get_dev_string_ascii(libusb_device_handle *dev, size_t size,
                                  uint8_t id)
{
	char *buf = malloc(size);
	int ret = libusb_get_string_descriptor_ascii(dev, id,
	                                             (unsigned char *) buf,
	                                             size);

	if (ret < 0) {
		free(buf);
		return strdup("(error)");
	}

	return buf;
}

static uint16_t get_any_langid(libusb_device_handle *dev)
{
	unsigned char buf[4];
	int ret = libusb_get_string_descriptor(dev, 0, 0, buf, sizeof buf);

	if (ret != sizeof buf)
		return 0;
	return buf[2] | (buf[3] << 8);
}

static char *usb_string_to_native(char * str, size_t len)
{
	size_t num_converted;
	iconv_t conv;
	char *result, *result_end;
	size_t in_bytes_left, out_bytes_left;

	conv = iconv_open(nl_langinfo(CODESET), "UTF-16LE");

	if (conv == (iconv_t) -1)
		return NULL;

	in_bytes_left = len * 2;
	out_bytes_left = len * MB_CUR_MAX;
	result = result_end = malloc(out_bytes_left + 1);

	num_converted = iconv(conv, &str, &in_bytes_left,
	                      &result_end, &out_bytes_left);

	iconv_close(conv);
	if (num_converted == (size_t) -1) {
		free(result);
		return NULL;
	}

	*result_end = 0;
	return result;
}

char *get_dev_string(libusb_device_handle *dev, uint8_t id)
{
	int ret;
	char *buf, unicode_buf[254];
	uint16_t langid;

	if (!dev || !id)
		return strdup("");

	langid = get_any_langid(dev);
	if (!langid)
		return strdup("(error)");

	/*
	 * Some devices lie about their string size, so initialize
	 * the buffer with all 0 to account for that.
	 */
	memset(unicode_buf, 0x00, sizeof(unicode_buf));

	ret = libusb_get_string_descriptor(dev, id, langid,
	                                   (unsigned char *) unicode_buf,
	                                   sizeof unicode_buf);
	if (ret < 2) return strdup("(error)");

	if ((unsigned char)unicode_buf[0] < 2 || unicode_buf[1] != LIBUSB_DT_STRING)
		return strdup("(error)");

	buf = usb_string_to_native(unicode_buf + 2,
	                           ((unsigned char) unicode_buf[0] - 2) / 2);
	if (!buf)
		return get_dev_string_ascii(dev, 127, id);

	return buf;
}

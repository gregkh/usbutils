/*****************************************************************************/
/*
 *      usbmisc.c  --  Misc USB routines
 *
 *      Copyright (C) 2003  Aurelien Jarno (aurelien@aurel32.net)
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
 *
 */

/*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifdef HAVE_NL_LANGINFO
#include <langinfo.h>
#endif

#include "usbmisc.h"

/* ---------------------------------------------------------------------- */

static const char *devbususb = "/dev/bus/usb";

/* ---------------------------------------------------------------------- */

static int readlink_recursive(const char *path, char *buf, size_t bufsize)
{
	char temp[PATH_MAX + 1];
	char *ptemp;
	int ret;

	ret = readlink(path, buf, bufsize-1);

	if (ret > 0) {
		buf[ret] = 0;
		if (*buf != '/') {
			strncpy(temp, path, sizeof(temp));
			ptemp = temp + strlen(temp);
			while (*ptemp != '/' && ptemp != temp)
				ptemp--;
			ptemp++;
			strncpy(ptemp, buf, bufsize + temp - ptemp);
		} else
			strncpy(temp, buf, sizeof(temp));
		return readlink_recursive(temp, buf, bufsize);
	} else {
		strncpy(buf, path, bufsize);
		return strlen(buf);
	}
}

static char *get_absolute_path(const char *path, char *result,
			       size_t result_size)
{
	const char *ppath;	/* pointer on the input string */
	char *presult;		/* pointer on the output string */

	ppath = path;
	presult = result;
	result[0] = 0;

	if (path == NULL)
		return result;

	if (*ppath != '/') {
		result = getcwd(result, result_size);
		presult += strlen(result);
		result_size -= strlen(result);

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

	return result;
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

	for (i = 0; i < num_devs; ++i) {
		uint8_t bnum = libusb_get_bus_number(list[i]);
		uint8_t dnum = libusb_get_device_address(list[i]);

		snprintf(device_path, sizeof(device_path), "%s/%03u/%03u",
			 devbususb, bnum, dnum);
		if (!strcmp(device_path, absolute_path)) {
			dev = list[i];
			break;
		}
	}

	libusb_free_device_list(list, 0);
	return dev;
}

static char *get_dev_string_ascii(libusb_device_handle *dev, size_t size,
                                  u_int8_t id)
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

#if defined(HAVE_NL_LANGINFO) && defined(HAVE_ICONV)
static u_int16_t get_any_langid(libusb_device_handle *dev)
{
	unsigned char buf[4];
	int ret = libusb_get_string_descriptor(dev, 0, 0, buf, sizeof buf);
	if (ret != sizeof buf) return 0;
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
#endif

char *get_dev_string(libusb_device_handle *dev, u_int8_t id)
{
#if defined(HAVE_NL_LANGINFO) && defined(HAVE_ICONV)
	int ret;
	char *buf, unicode_buf[254];
	u_int16_t langid;
#endif

	if (!dev || !id) return strdup("");

#if defined(HAVE_NL_LANGINFO) && defined(HAVE_ICONV)
	langid = get_any_langid(dev);
	if (!langid) return strdup("(error)");

	ret = libusb_get_string_descriptor(dev, id, langid,
	                                   (unsigned char *) unicode_buf,
	                                   sizeof unicode_buf);
	if (ret < 2) return strdup("(error)");

	if (unicode_buf[0] < 2 || unicode_buf[1] != LIBUSB_DT_STRING)
		return strdup("(error)");

	buf = usb_string_to_native(unicode_buf + 2,
	                           ((unsigned char) unicode_buf[0] - 2) / 2);

	if (!buf) return get_dev_string_ascii(dev, 127, id);

	return buf;
#else
	return get_dev_string_ascii(dev, 127, id);
#endif
}

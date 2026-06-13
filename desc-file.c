// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * USB descriptor file parsing
 *
 * Copyright (C) 2026 Google LLC
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libusb.h>
#include "desc-file.h"

/*
 * parse_config_descriptor - Parse a configuration descriptor from a buffer.
 * @buf: The buffer containing the configuration descriptor.
 * @size: The size of the buffer.
 *
 * This function parses a raw configuration descriptor from a buffer and
 * populates a libusb_config_descriptor structure.
 *
 * Returns a pointer to the allocated configuration descriptor, or NULL on failure.
 */
static struct libusb_config_descriptor *parse_config_descriptor(const unsigned char *buf, int size)
{
	struct libusb_config_descriptor *cfg;
	struct libusb_interface *ifc = NULL;
	struct libusb_interface_descriptor *alt = NULL;
	struct libusb_endpoint_descriptor *ep = NULL;

	if (size < 9 || buf[1] != LIBUSB_DT_CONFIG)
		return NULL;

	cfg = calloc(1, sizeof(*cfg));
	if (!cfg) {
		perror("calloc");
		return NULL;
	}

	cfg->bLength = buf[0];
	cfg->bDescriptorType = buf[1];
	cfg->wTotalLength = buf[2] | (buf[3] << 8);
	cfg->bNumInterfaces = buf[4];
	cfg->bConfigurationValue = buf[5];
	cfg->iConfiguration = buf[6];
	cfg->bmAttributes = buf[7];
	cfg->MaxPower = buf[8];

	if (cfg->bNumInterfaces > 0) {
		cfg->interface = calloc(cfg->bNumInterfaces, sizeof(*cfg->interface));
		if (!cfg->interface) {
			perror("calloc");
			desc_file_free_config_descriptor(cfg);
			return NULL;
		}
	}

	buf += cfg->bLength;
	size -= cfg->bLength;

	while (size >= 2) {
		unsigned char len = buf[0];
		unsigned char type = buf[1];
		void *tmp;
		int ep_idx;

		if (len > size || len < 2)
			break;

		switch (type) {
		case LIBUSB_DT_INTERFACE:
			if (buf[2] >= cfg->bNumInterfaces)
				break;
			ifc = (struct libusb_interface *)&cfg->interface[buf[2]];
			ifc->num_altsetting++;
			tmp = realloc((void *)ifc->altsetting, ifc->num_altsetting * sizeof(*ifc->altsetting));
			if (!tmp) {
				perror("realloc");
				desc_file_free_config_descriptor(cfg);
				return NULL;
			}
			ifc->altsetting = tmp;
			alt = (struct libusb_interface_descriptor *)&ifc->altsetting[ifc->num_altsetting - 1];
			memset(alt, 0, sizeof(*alt));
			alt->bLength = buf[0];
			alt->bDescriptorType = buf[1];
			alt->bInterfaceNumber = buf[2];
			alt->bAlternateSetting = buf[3];
			alt->bNumEndpoints = buf[4];
			alt->bInterfaceClass = buf[5];
			alt->bInterfaceSubClass = buf[6];
			alt->bInterfaceProtocol = buf[7];
			alt->iInterface = buf[8];
			if (alt->bNumEndpoints > 0) {
				alt->endpoint = calloc(alt->bNumEndpoints, sizeof(*alt->endpoint));
				if (!alt->endpoint) {
					perror("calloc");
					desc_file_free_config_descriptor(cfg);
					return NULL;
				}
			}
			ep = NULL;
			break;

		case LIBUSB_DT_ENDPOINT:
			if (!(alt && alt->endpoint))
				break;
			ep_idx = 0;
			/* Find next free endpoint slot */
			while (ep_idx < alt->bNumEndpoints && alt->endpoint[ep_idx].bLength)
				ep_idx++;
			if (ep_idx < alt->bNumEndpoints) {
				ep = (struct libusb_endpoint_descriptor *)&alt->endpoint[ep_idx];
				ep->bLength = buf[0];
				ep->bDescriptorType = buf[1];
				ep->bEndpointAddress = buf[2];
				ep->bmAttributes = buf[3];
				ep->wMaxPacketSize = buf[4] | (buf[5] << 8);
				ep->bInterval = buf[6];
				ep->bRefresh = (len > 7) ? buf[7] : 0;
				ep->bSynchAddress = (len > 8) ? buf[8] : 0;
			}
			break;

		default:
			/* Extra descriptors */
			if (ep) {
				tmp = realloc((void *)ep->extra, ep->extra_length + len);
				if (!tmp) {
					perror("realloc");
					desc_file_free_config_descriptor(cfg);
					return NULL;
				}
				ep->extra = tmp;
				memcpy((void *)(ep->extra + ep->extra_length), buf, len);
				ep->extra_length += len;
			} else if (alt) {
				tmp = realloc((void *)alt->extra, alt->extra_length + len);
				if (!tmp) {
					perror("realloc");
					desc_file_free_config_descriptor(cfg);
					return NULL;
				}
				alt->extra = tmp;
				memcpy((void *)(alt->extra + alt->extra_length), buf, len);
				alt->extra_length += len;
			} else if (cfg) {
				tmp = realloc((void *)cfg->extra, cfg->extra_length + len);
				if (!tmp) {
					perror("realloc");
					desc_file_free_config_descriptor(cfg);
					return NULL;
				}
				cfg->extra = tmp;
				memcpy((void *)(cfg->extra + cfg->extra_length), buf, len);
				cfg->extra_length += len;
			}
			break;
		}

		buf += len;
		size -= len;
	}

	return cfg;
}

/*
 * desc_file_free_config_descriptor - Free a configuration descriptor.
 * @cfg: The configuration descriptor to free.
 */
void desc_file_free_config_descriptor(struct libusb_config_descriptor *cfg)
{
	if (!cfg)
		return;
	for (int i = 0; i < cfg->bNumInterfaces; i++) {
		struct libusb_interface *ifc = (struct libusb_interface *)&cfg->interface[i];
		if (!ifc)
			continue;
		for (int j = 0; j < ifc->num_altsetting; j++) {
			struct libusb_interface_descriptor *alt =
				(struct libusb_interface_descriptor *)&ifc->altsetting[j];
			if (!alt)
				continue;
			for (int k = 0; k < alt->bNumEndpoints; k++) {
				struct libusb_endpoint_descriptor *ep =
					(struct libusb_endpoint_descriptor *)&alt->endpoint[k];
				if (!ep)
					continue;
				if (ep->extra)
					free((void *)ep->extra);
			}
			if (alt->endpoint)
				free((void *)alt->endpoint);
			if (alt->extra)
				free((void *)alt->extra);
		}
		if (ifc->altsetting)
			free((void *)ifc->altsetting);
	}
	if (cfg->interface)
		free((void *)cfg->interface);
	if (cfg->extra)
		free((void *)cfg->extra);
	free(cfg);
}

/*
 * desc_file_get_device_descriptor - Get a device descriptor from a file.
 * @fd: The file descriptor to read from.
 * @desc: The device descriptor to populate.
 *
 * Returns 0 on success, or -1 on failure.
 */
int desc_file_get_device_descriptor(int fd, struct libusb_device_descriptor *desc)
{
	unsigned char buf[18];
	int n;

	n = read(fd, buf, sizeof(buf));
	if (n < (int)sizeof(buf)) {
		fprintf(stderr, "File too short for device descriptor\n");
		return -1;
	}

	/* Populate device descriptor */
	desc->bLength = buf[0];
	desc->bDescriptorType = buf[1];
	desc->bcdUSB = buf[2] | (buf[3] << 8);
	desc->bDeviceClass = buf[4];
	desc->bDeviceSubClass = buf[5];
	desc->bDeviceProtocol = buf[6];
	desc->bMaxPacketSize0 = buf[7];
	desc->idVendor = buf[8] | (buf[9] << 8);
	desc->idProduct = buf[10] | (buf[11] << 8);
	desc->bcdDevice = buf[12] | (buf[13] << 8);
	desc->iManufacturer = buf[14];
	desc->iProduct = buf[15];
	desc->iSerialNumber = buf[16];
	desc->bNumConfigurations = buf[17];

	return 0;
}

/*
 * desc_file_get_next_config_descriptor - Get the next config descriptor from a file.
 * @fd: The file descriptor to read from.
 *
 * Returns a pointer to the allocated configuration descriptor, or NULL on failure.
 */
struct libusb_config_descriptor *desc_file_get_next_config_descriptor(int fd)
{
	unsigned char config_header[9];
	struct libusb_config_descriptor *config;
	unsigned char *config_buf;
	uint16_t total_length;
	ssize_t n;

	n = read(fd, config_header, sizeof(config_header));
	if (n < (ssize_t)sizeof(config_header)) {
		if (n > 0)
			fprintf(stderr, "File too short for config header\n");
		return NULL;
	}

	total_length = config_header[2] | (config_header[3] << 8);
	config_buf = malloc(total_length);
	if (!config_buf) {
		perror("malloc");
		return NULL;
	}

	memcpy(config_buf, config_header, sizeof(config_header));
	if (total_length > sizeof(config_header)) {
		n = read(fd, config_buf + sizeof(config_header), total_length - sizeof(config_header));
		if (n < (ssize_t)(total_length - sizeof(config_header))) {
			fprintf(stderr, "File too short for config\n");
			free(config_buf);
			return NULL;
		}
	}

	config = parse_config_descriptor(config_buf, total_length);
	if (!config) {
		fprintf(stderr, "Failed to parse config descriptor\n");
		free(config_buf);
		return NULL;
	}

	free(config_buf);
	return config;
}

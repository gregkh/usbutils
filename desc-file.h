// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * USB descriptor file parsing
 *
 * Copyright (C) 2026 Google LLC
 */

#ifndef _DESC_FILE_H
#define _DESC_FILE_H

#include <libusb.h>

/**
 * desc_file_free_config_descriptor:
 * @cfg: The configuration descriptor to free.
 *
 * This function frees a configuration descriptor and all its sub-descriptors.
 */
void desc_file_free_config_descriptor(struct libusb_config_descriptor *cfg);

/**
 * desc_file_get_device_descriptor:
 * @fd: File descriptor to read from.
 * @desc: Pointer to a device descriptor to populate.
 *
 * Reads a USB device descriptor from a file descriptor.
 *
 * Returns 0 on success, or -1 on failure.
 */
int desc_file_get_device_descriptor(int fd, struct libusb_device_descriptor *desc);

/**
 * desc_file_get_next_config_descriptor:
 * @fd: File descriptor to read from.
 *
 * Reads the next configuration descriptor from a file descriptor.
 *
 * Returns a pointer to the allocated configuration descriptor, or NULL on failure.
 */
struct libusb_config_descriptor *desc_file_get_next_config_descriptor(int fd);

#endif /* _DESC_FILE_H */

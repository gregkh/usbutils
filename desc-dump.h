/*****************************************************************************/

/*
 *      desc-dump.h  --  USB descriptor dumping
 *
 *      Copyright (C) 2017 Michael Drake <michael.drake@codethink.co.uk>
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

#ifndef _DESC_DUMP_H
#define _DESC_DUMP_H

/* ---------------------------------------------------------------------- */

/**
 * Buffer length value indicating that the buffer length should be
 * read from the value of the first field in the buffer, as defined
 * by the first field descriptor definition.
 */
#define DESC_BUF_LEN_FROM_BUF 0xffffffff

/**
 * Dump descriptor using a descriptor definition array.
 *
 * This function dumps the USB descriptor data given in the byte array,
 * `buf`, according to the descriptor definition array given in `desc`.
 *
 * The first byte(s) of `buf` must correspond to the first field definition
 * in the `desc` descriptor definition array.
 *
 * \param[in] dev     LibUSB device handle.
 * \param[in] desc    Array of descriptor field definitions to use to interpret
 *                    `buf`.  This array constitutes the descriptor definition.
 *                    The final entry in the array must have a NULL field name,
 *                    which is interpreted as the end of the array.
 * \param[in] buf     Byte array containing the descriptor data to dump.
 * \param[in] buf_len Byte length of `buf` or `DESC_BUF_LEN_FROM_BUF` to get
 *                    the length from the value of the first field in the
 *                    descriptor data.
 * \param[in] indent  Indent level to use for descriptor dump.
 */
extern void desc_dump(
		libusb_device_handle *dev,
		const struct desc *desc,
		const unsigned char *buf,
		unsigned int buf_len,
		unsigned int indent);


/* ---------------------------------------------------------------------- */

#endif /* _DESC_DUMP_H */

/*****************************************************************************/

/*
 *      lsusb.c  --  lspci like utility for the USB bus
 *
 *      Copyright (C) 1999-2001, 2003
 *        Thomas Sailer (t.sailer@alumni.ethz.ch)
 *      Copyright (C) 2003-2005 David Brownell
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

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <locale.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#include <libusb.h>
#include <unistd.h>

#include "lsusb.h"
#include "names.h"
#include "usbmisc.h"

#include <getopt.h>

#define le16_to_cpu(x) libusb_cpu_to_le16(libusb_cpu_to_le16(x))

/* from USB 2.0 spec and updates */
#define USB_DT_DEVICE_QUALIFIER		0x06
#define USB_DT_OTHER_SPEED_CONFIG	0x07
#define USB_DT_OTG			0x09
#define USB_DT_DEBUG			0x0a
#define USB_DT_INTERFACE_ASSOCIATION	0x0b
#define USB_DT_SECURITY			0x0c
#define USB_DT_KEY			0x0d
#define USB_DT_ENCRYPTION_TYPE		0x0e
#define USB_DT_BOS			0x0f
#define USB_DT_DEVICE_CAPABILITY	0x10
#define USB_DT_WIRELESS_ENDPOINT_COMP	0x11
#define USB_DT_WIRE_ADAPTER		0x21
#define USB_DT_RPIPE			0x22
#define USB_DT_RC_INTERFACE		0x23
#define USB_DT_SS_ENDPOINT_COMP		0x30

/* Device Capability Type Codes (Wireless USB spec and USB 3.0 bus spec) */
#define USB_DC_WIRELESS_USB		0x01
#define USB_DC_20_EXTENSION		0x02
#define USB_DC_SUPERSPEED		0x03
#define USB_DC_CONTAINER_ID		0x04

/* Conventional codes for class-specific descriptors.  The convention is
 * defined in the USB "Common Class" Spec (3.11).  Individual class specs
 * are authoritative for their usage, not the "common class" writeup.
 */
#define USB_DT_CS_DEVICE		(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_DT_DEVICE)
#define USB_DT_CS_CONFIG		(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_DT_CONFIG)
#define USB_DT_CS_STRING		(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_DT_STRING)
#define USB_DT_CS_INTERFACE		(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_DT_INTERFACE)
#define USB_DT_CS_ENDPOINT		(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_DT_ENDPOINT)

#ifndef USB_CLASS_CCID
#define USB_CLASS_CCID			0x0b
#endif

#ifndef USB_CLASS_VIDEO
#define USB_CLASS_VIDEO			0x0e
#endif

#ifndef USB_CLASS_APPLICATION
#define USB_CLASS_APPLICATION	       	0xfe
#endif

#ifndef USB_AUDIO_CLASS_1
#define USB_AUDIO_CLASS_1		0x00
#endif

#ifndef USB_AUDIO_CLASS_2
#define USB_AUDIO_CLASS_2		0x20
#endif

#define VERBLEVEL_DEFAULT 0	/* 0 gives lspci behaviour; 1, lsusb-0.9 */

#define CTRL_RETRIES	 2
#define CTRL_TIMEOUT	(5*1000)	/* milliseconds */

#define	HUB_STATUS_BYTELEN	3	/* max 3 bytes status = hub + 23 ports */

static const char procbususb[] = "/proc/bus/usb";
static unsigned int verblevel = VERBLEVEL_DEFAULT;
static int do_report_desc = 1;
static const char * const encryption_type[] = {
	"UNSECURE",
	"WIRED",
	"CCM_1",
	"RSA_1",
	"RESERVED"
};

static void dump_interface(libusb_device_handle *dev, const struct libusb_interface *interface);
static void dump_endpoint(libusb_device_handle *dev, const struct libusb_interface_descriptor *interface, const struct libusb_endpoint_descriptor *endpoint);
static void dump_audiocontrol_interface(libusb_device_handle *dev, const unsigned char *buf, int protocol);
static void dump_audiostreaming_interface(libusb_device_handle *dev, const unsigned char *buf, int protocol);
static void dump_midistreaming_interface(libusb_device_handle *dev, const unsigned char *buf);
static void dump_videocontrol_interface(libusb_device_handle *dev, const unsigned char *buf);
static void dump_videostreaming_interface(const unsigned char *buf);
static void dump_dfu_interface(const unsigned char *buf);
static char *dump_comm_descriptor(libusb_device_handle *dev, const unsigned char *buf, char *indent);
static void dump_hid_device(libusb_device_handle *dev, const struct libusb_interface_descriptor *interface, const unsigned char *buf);
static void dump_audiostreaming_endpoint(const unsigned char *buf, int protocol);
static void dump_midistreaming_endpoint(const unsigned char *buf);
static void dump_hub(const char *prefix, const unsigned char *p, int tt_type);
static void dump_ccid_device(const unsigned char *buf);

/* ---------------------------------------------------------------------- */

static unsigned int convert_le_u32 (const unsigned char *buf)
{
	return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

/* ---------------------------------------------------------------------- */

/* workaround libusb API goofs:  "byte" should never be sign extended;
 * using "char" is trouble.
 */

static inline int typesafe_control_msg(libusb_device_handle *dev,
	unsigned char requesttype, unsigned char request,
	int value, int idx,
	unsigned char *bytes, unsigned size, int timeout)
{
	int ret = libusb_control_transfer(dev, requesttype, request, value,
					idx, bytes, size, timeout);

	return ret;
}

#define usb_control_msg		typesafe_control_msg

static int get_protocol_string(char *buf, size_t size, u_int8_t cls, u_int8_t subcls, u_int8_t proto)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_protocol(cls, subcls, proto)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

static int get_audioterminal_string(char *buf, size_t size, u_int16_t termt)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_audioterminal(termt)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

static int get_videoterminal_string(char *buf, size_t size, u_int16_t termt)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_videoterminal(termt)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

static const char *get_guid(const unsigned char *buf)
{
	static char guid[39];

	/* NOTE:  see RFC 4122 for more information about GUID/UUID
	 * structure.  The first fields fields are historically big
	 * endian numbers, dating from Apollo mc68000 workstations.
	 */
	sprintf(guid, "{%02x%02x%02x%02x"
			"-%02x%02x"
			"-%02x%02x"
			"-%02x%02x"
			"-%02x%02x%02x%02x%02x%02x}",
	       buf[0], buf[1], buf[2], buf[3],
	       buf[4], buf[5],
	       buf[6], buf[7],
	       buf[8], buf[9],
	       buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
	return guid;
}

/* ---------------------------------------------------------------------- */

static void dump_bytes(const unsigned char *buf, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i++)
		printf(" %02x", buf[i]);
	printf("\n");
}

static void dump_junk(const unsigned char *buf, const char *indent, unsigned int len)
{
	unsigned int i;

	if (buf[0] <= len)
		return;
	printf("%sjunk at descriptor end:", indent);
	for (i = len; i < buf[0]; i++)
		printf(" %02x", buf[i]);
	printf("\n");
}

/*
 * General config descriptor dump
 */

static void dump_device(
	libusb_device_handle *dev,
	struct libusb_device_descriptor *descriptor
)
{
	char vendor[128], product[128];
	char cls[128], subcls[128], proto[128];
	char *mfg, *prod, *serial;

	get_vendor_string(vendor, sizeof(vendor), descriptor->idVendor);
	get_product_string(product, sizeof(product),
			descriptor->idVendor, descriptor->idProduct);
	get_class_string(cls, sizeof(cls), descriptor->bDeviceClass);
	get_subclass_string(subcls, sizeof(subcls),
			descriptor->bDeviceClass, descriptor->bDeviceSubClass);
	get_protocol_string(proto, sizeof(proto), descriptor->bDeviceClass,
			descriptor->bDeviceSubClass, descriptor->bDeviceProtocol);

	mfg = get_dev_string(dev, descriptor->iManufacturer);
	prod = get_dev_string(dev, descriptor->iProduct);
	serial = get_dev_string(dev, descriptor->iSerialNumber);

	printf("Device Descriptor:\n"
	       "  bLength             %5u\n"
	       "  bDescriptorType     %5u\n"
	       "  bcdUSB              %2x.%02x\n"
	       "  bDeviceClass        %5u %s\n"
	       "  bDeviceSubClass     %5u %s\n"
	       "  bDeviceProtocol     %5u %s\n"
	       "  bMaxPacketSize0     %5u\n"
	       "  idVendor           0x%04x %s\n"
	       "  idProduct          0x%04x %s\n"
	       "  bcdDevice           %2x.%02x\n"
	       "  iManufacturer       %5u %s\n"
	       "  iProduct            %5u %s\n"
	       "  iSerial             %5u %s\n"
	       "  bNumConfigurations  %5u\n",
	       descriptor->bLength, descriptor->bDescriptorType,
	       descriptor->bcdUSB >> 8, descriptor->bcdUSB & 0xff,
	       descriptor->bDeviceClass, cls,
	       descriptor->bDeviceSubClass, subcls,
	       descriptor->bDeviceProtocol, proto,
	       descriptor->bMaxPacketSize0,
	       descriptor->idVendor, vendor, descriptor->idProduct, product,
	       descriptor->bcdDevice >> 8, descriptor->bcdDevice & 0xff,
	       descriptor->iManufacturer, mfg,
	       descriptor->iProduct, prod,
	       descriptor->iSerialNumber, serial,
	       descriptor->bNumConfigurations);

	free(mfg);
	free(prod);
	free(serial);
}

static void dump_wire_adapter(const unsigned char *buf)
{

	printf("      Wire Adapter Class Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bcdWAVersion        %2x.%02x\n"
	       "	 bNumPorts	     %5u\n"
	       "	 bmAttributes	     %5u\n"
	       "	 wNumRPRipes	     %5u\n"
	       "	 wRPipeMaxBlock	     %5u\n"
	       "	 bRPipeBlockSize     %5u\n"
	       "	 bPwrOn2PwrGood	     %5u\n"
	       "	 bNumMMCIEs	     %5u\n"
	       "	 DeviceRemovable     %5u\n",
	       buf[0], buf[1], buf[3], buf[2], buf[4], buf[5],
	       (buf[6] | buf[7] << 8),
	       (buf[8] | buf[9] << 8),
	       buf[10], buf[11], buf[12], buf[13]);
}

static void dump_rc_interface(const unsigned char *buf)
{
	printf("      Radio Control Interface Class Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bcdRCIVersion       %2x.%02x\n",
	       buf[0], buf[1], buf[3], buf[2]);

}

static void dump_security(const unsigned char *buf)
{
	printf("    Security Descriptor:\n"
	       "      bLength             %5u\n"
	       "      bDescriptorType     %5u\n"
	       "      wTotalLength        %5u\n"
	       "      bNumEncryptionTypes %5u\n",
	       buf[0], buf[1], (buf[3] << 8 | buf[2]), buf[4]);
}

static void dump_encryption_type(const unsigned char *buf)
{
	int b_encryption_type = buf[2] & 0x4;

	printf("    Encryption Type Descriptor:\n"
	       "      bLength             %5u\n"
	       "      bDescriptorType     %5u\n"
	       "      bEncryptionType     %5u %s\n"
	       "      bEncryptionValue    %5u\n"
	       "      bAuthKeyIndex       %5u\n",
	       buf[0], buf[1], buf[2],
	       encryption_type[b_encryption_type], buf[3], buf[4]);
}

static void dump_association(libusb_device_handle *dev, const unsigned char *buf)
{
	char cls[128], subcls[128], proto[128];
	char *func;

	get_class_string(cls, sizeof(cls), buf[4]);
	get_subclass_string(subcls, sizeof(subcls), buf[4], buf[5]);
	get_protocol_string(proto, sizeof(proto), buf[4], buf[5], buf[6]);
	func = get_dev_string(dev, buf[7]);

	printf("    Interface Association:\n"
	       "      bLength             %5u\n"
	       "      bDescriptorType     %5u\n"
	       "      bFirstInterface     %5u\n"
	       "      bInterfaceCount     %5u\n"
	       "      bFunctionClass      %5u %s\n"
	       "      bFunctionSubClass   %5u %s\n"
	       "      bFunctionProtocol   %5u %s\n"
	       "      iFunction           %5u %s\n",
	       buf[0], buf[1],
	       buf[2], buf[3],
	       buf[4], cls,
	       buf[5], subcls,
	       buf[6], proto,
	       buf[7], func);

	free(func);
}

static void dump_config(libusb_device_handle *dev, struct libusb_config_descriptor *config)
{
	char *cfg;
	int i;

	cfg = get_dev_string(dev, config->iConfiguration);

	printf("  Configuration Descriptor:\n"
	       "    bLength             %5u\n"
	       "    bDescriptorType     %5u\n"
	       "    wTotalLength        %5u\n"
	       "    bNumInterfaces      %5u\n"
	       "    bConfigurationValue %5u\n"
	       "    iConfiguration      %5u %s\n"
	       "    bmAttributes         0x%02x\n",
	       config->bLength, config->bDescriptorType,
	       le16_to_cpu(config->wTotalLength),
	       config->bNumInterfaces, config->bConfigurationValue,
	       config->iConfiguration,
	       cfg, config->bmAttributes);

	free(cfg);

	if (!(config->bmAttributes & 0x80))
		printf("      (Missing must-be-set bit!)\n");
	if (config->bmAttributes & 0x40)
		printf("      Self Powered\n");
	else
		printf("      (Bus Powered)\n");
	if (config->bmAttributes & 0x20)
		printf("      Remote Wakeup\n");
	if (config->bmAttributes & 0x10)
		printf("      Battery Powered\n");
	printf("    MaxPower            %5umA\n", config->MaxPower * 2);

	/* avoid re-ordering or hiding descriptors for display */
	if (config->extra_length) {
		int		size = config->extra_length;
		const unsigned char	*buf = config->extra;

		while (size >= 2) {
			if (buf[0] < 2) {
				dump_junk(buf, "        ", size);
				break;
			}
			switch (buf[1]) {
			case USB_DT_OTG:
				/* handled separately */
				break;
			case USB_DT_INTERFACE_ASSOCIATION:
				dump_association(dev, buf);
				break;
			case USB_DT_SECURITY:
				dump_security(buf);
				break;
			case USB_DT_ENCRYPTION_TYPE:
				dump_encryption_type(buf);
				break;
			default:
				/* often a misplaced class descriptor */
				printf("    ** UNRECOGNIZED: ");
				dump_bytes(buf, buf[0]);
				break;
			}
			size -= buf[0];
			buf += buf[0];
		}
	}
	for (i = 0 ; i < config->bNumInterfaces ; i++)
		dump_interface(dev, &config->interface[i]);
}

static void dump_altsetting(libusb_device_handle *dev, const struct libusb_interface_descriptor *interface)
{
	char cls[128], subcls[128], proto[128];
	char *ifstr;

	const unsigned char *buf;
	unsigned size, i;

	get_class_string(cls, sizeof(cls), interface->bInterfaceClass);
	get_subclass_string(subcls, sizeof(subcls), interface->bInterfaceClass, interface->bInterfaceSubClass);
	get_protocol_string(proto, sizeof(proto), interface->bInterfaceClass, interface->bInterfaceSubClass, interface->bInterfaceProtocol);
	ifstr = get_dev_string(dev, interface->iInterface);

	printf("    Interface Descriptor:\n"
	       "      bLength             %5u\n"
	       "      bDescriptorType     %5u\n"
	       "      bInterfaceNumber    %5u\n"
	       "      bAlternateSetting   %5u\n"
	       "      bNumEndpoints       %5u\n"
	       "      bInterfaceClass     %5u %s\n"
	       "      bInterfaceSubClass  %5u %s\n"
	       "      bInterfaceProtocol  %5u %s\n"
	       "      iInterface          %5u %s\n",
	       interface->bLength, interface->bDescriptorType, interface->bInterfaceNumber,
	       interface->bAlternateSetting, interface->bNumEndpoints, interface->bInterfaceClass, cls,
	       interface->bInterfaceSubClass, subcls, interface->bInterfaceProtocol, proto,
	       interface->iInterface, ifstr);

	free(ifstr);

	/* avoid re-ordering or hiding descriptors for display */
	if (interface->extra_length) {
		size = interface->extra_length;
		buf = interface->extra;
		while (size >= 2 * sizeof(u_int8_t)) {
			if (buf[0] < 2) {
				dump_junk(buf, "      ", size);
				break;
			}

			switch (buf[1]) {

			/* This is the polite way to provide class specific
			 * descriptors: explicitly tagged, using common class
			 * spec conventions.
			 */
			case USB_DT_CS_DEVICE:
			case USB_DT_CS_INTERFACE:
				switch (interface->bInterfaceClass) {
				case LIBUSB_CLASS_AUDIO:
					switch (interface->bInterfaceSubClass) {
					case 1:
						dump_audiocontrol_interface(dev, buf, interface->bInterfaceProtocol);
						break;
					case 2:
						dump_audiostreaming_interface(dev, buf, interface->bInterfaceProtocol);
						break;
					case 3:
						dump_midistreaming_interface(dev, buf);
						break;
					default:
						goto dump;
					}
					break;
				case LIBUSB_CLASS_COMM:
					dump_comm_descriptor(dev, buf,
						"      ");
					break;
				case USB_CLASS_VIDEO:
					switch (interface->bInterfaceSubClass) {
					case 1:
						dump_videocontrol_interface(dev, buf);
						break;
					case 2:
						dump_videostreaming_interface(buf);
						break;
					default:
						goto dump;
					}
					break;
				case USB_CLASS_APPLICATION:
					switch (interface->bInterfaceSubClass) {
					case 1:
						dump_dfu_interface(buf);
						break;
					default:
						goto dump;
					}
					break;
				case LIBUSB_CLASS_HID:
					dump_hid_device(dev, interface, buf);
					break;
				case USB_CLASS_CCID:
					dump_ccid_device(buf);
					break;
				default:
					goto dump;
				}
				break;

			/* This is the ugly way:  implicitly tagged,
			 * each class could redefine the type IDs.
			 */
			default:
				switch (interface->bInterfaceClass) {
				case LIBUSB_CLASS_HID:
					dump_hid_device(dev, interface, buf);
					break;
				case USB_CLASS_CCID:
					dump_ccid_device(buf);
					break;
				case 0xe0:	/* wireless */
					switch (interface->bInterfaceSubClass) {
					case 1:
						switch (interface->bInterfaceProtocol) {
						case 2:
							dump_rc_interface(buf);
							break;
						default:
							goto dump;
						}
						break;
					case 2:
						dump_wire_adapter(buf);
						break;
					default:
						goto dump;
					}
					break;
				case LIBUSB_CLASS_AUDIO:
					switch (buf[1]) {
					/* MISPLACED DESCRIPTOR */
					case USB_DT_CS_ENDPOINT:
						switch (interface->bInterfaceSubClass) {
						case 2:
							dump_audiostreaming_endpoint(buf, interface->bInterfaceProtocol);
							break;
						default:
							goto dump;
						}
						break;
					default:
						goto dump;
					}
					break;
				default:
					/* ... not everything is class-specific */
					switch (buf[1]) {
					case USB_DT_OTG:
						/* handled separately */
						break;
					case USB_DT_INTERFACE_ASSOCIATION:
						dump_association(dev, buf);
						break;
					default:
dump:
						/* often a misplaced class descriptor */
						printf("      ** UNRECOGNIZED: ");
						dump_bytes(buf, buf[0]);
						break;
					}
				}
			}
			size -= buf[0];
			buf += buf[0];
		}
	}

	for (i = 0 ; i < interface->bNumEndpoints ; i++)
		dump_endpoint(dev, interface, &interface->endpoint[i]);
}

static void dump_interface(libusb_device_handle *dev, const struct libusb_interface *interface)
{
	int i;

	for (i = 0; i < interface->num_altsetting; i++)
		dump_altsetting(dev, &interface->altsetting[i]);
}

static void dump_pipe_desc(const unsigned char *buf)
{
	static const char *pipe_name[] = {
		"Reserved",
		"Command pipe",
		"Status pipe",
		"Data-in pipe",
		"Data-out pipe",
		[5 ... 0xDF] = "Reserved",
		[0xE0 ... 0xEF] = "Vendor specific",
		[0xF0 ... 0xFF] = "Reserved",
	};
	
	if (buf[0] == 4 && buf[1] == 0x24) {
		printf("        %s (0x%02x)\n", pipe_name[buf[2]], buf[2]);
	} else {
		printf("        INTERFACE CLASS: ");
		dump_bytes(buf, buf[0]);
	}
}

static void dump_endpoint(libusb_device_handle *dev, const struct libusb_interface_descriptor *interface, const struct libusb_endpoint_descriptor *endpoint)
{
	static const char * const typeattr[] = {
		"Control",
		"Isochronous",
		"Bulk",
		"Interrupt"
	};
	static const char * const syncattr[] = {
		"None",
		"Asynchronous",
		"Adaptive",
		"Synchronous"
	};
	static const char * const usage[] = {
		"Data",
		"Feedback",
		"Implicit feedback Data",
		"(reserved)"
	};
	static const char * const hb[] = { "1x", "2x", "3x", "(?\?)" };
	const unsigned char *buf;
	unsigned size;
	unsigned wmax = le16_to_cpu(endpoint->wMaxPacketSize);

	printf("      Endpoint Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bEndpointAddress     0x%02x  EP %u %s\n"
	       "        bmAttributes        %5u\n"
	       "          Transfer Type            %s\n"
	       "          Synch Type               %s\n"
	       "          Usage Type               %s\n"
	       "        wMaxPacketSize     0x%04x  %s %d bytes\n"
	       "        bInterval           %5u\n",
	       endpoint->bLength,
	       endpoint->bDescriptorType,
	       endpoint->bEndpointAddress,
	       endpoint->bEndpointAddress & 0x0f,
	       (endpoint->bEndpointAddress & 0x80) ? "IN" : "OUT",
	       endpoint->bmAttributes,
	       typeattr[endpoint->bmAttributes & 3],
	       syncattr[(endpoint->bmAttributes >> 2) & 3],
	       usage[(endpoint->bmAttributes >> 4) & 3],
	       wmax, hb[(wmax >> 11) & 3], wmax & 0x7ff,
	       endpoint->bInterval);
	/* only for audio endpoints */
	if (endpoint->bLength == 9)
		printf("        bRefresh            %5u\n"
		       "        bSynchAddress       %5u\n",
		       endpoint->bRefresh, endpoint->bSynchAddress);

	/* avoid re-ordering or hiding descriptors for display */
	if (endpoint->extra_length) {
		size = endpoint->extra_length;
		buf = endpoint->extra;
		while (size >= 2 * sizeof(u_int8_t)) {
			if (buf[0] < 2) {
				dump_junk(buf, "        ", size);
				break;
			}
			switch (buf[1]) {
			case USB_DT_CS_ENDPOINT:
				if (interface->bInterfaceClass == 1 && interface->bInterfaceSubClass == 2)
					dump_audiostreaming_endpoint(buf, interface->bInterfaceProtocol);
				else if (interface->bInterfaceClass == 1 && interface->bInterfaceSubClass == 3)
					dump_midistreaming_endpoint(buf);
				break;
			case USB_DT_CS_INTERFACE:
				/* MISPLACED DESCRIPTOR ... less indent */
				switch (interface->bInterfaceClass) {
				case LIBUSB_CLASS_COMM:
				case LIBUSB_CLASS_DATA:	/* comm data */
					dump_comm_descriptor(dev, buf,
						"      ");
					break;
				case LIBUSB_CLASS_MASS_STORAGE:
					dump_pipe_desc(buf);
					break;
				default:
					printf("        INTERFACE CLASS: ");
					dump_bytes(buf, buf[0]);
				}
				break;
			case USB_DT_CS_DEVICE:
				/* MISPLACED DESCRIPTOR ... less indent */
				switch (interface->bInterfaceClass) {
				case USB_CLASS_CCID:
					dump_ccid_device(buf);
					break;
				default:
					printf("        DEVICE CLASS: ");
					dump_bytes(buf, buf[0]);
				}
				break;
			case USB_DT_OTG:
				/* handled separately */
				break;
			case USB_DT_INTERFACE_ASSOCIATION:
				dump_association(dev, buf);
				break;
			case USB_DT_SS_ENDPOINT_COMP:
				printf("        bMaxBurst %15u\n", buf[2]);
				/* Print bulk streams info or isoc "Mult" */
				if ((endpoint->bmAttributes & 3) == 2 &&
						(buf[3] & 0x1f))
					printf("        MaxStreams %14u\n",
							(unsigned) 1 << buf[3]);
				if ((endpoint->bmAttributes & 3) == 1 &&
						(buf[3] & 0x3))
					printf("        Mult %20u\n",
							buf[3] & 0x3);
				break;
			default:
				/* often a misplaced class descriptor */
				printf("        ** UNRECOGNIZED: ");
				dump_bytes(buf, buf[0]);
				break;
			}
			size -= buf[0];
			buf += buf[0];
		}
	}
}

static void dump_unit(unsigned int data, unsigned int len)
{
	char *systems[5] = { "None", "SI Linear", "SI Rotation",
			"English Linear", "English Rotation" };

	char *units[5][8] = {
		{ "None", "None", "None", "None", "None",
				"None", "None", "None" },
		{ "None", "Centimeter", "Gram", "Seconds", "Kelvin",
				"Ampere", "Candela", "None" },
		{ "None", "Radians",    "Gram", "Seconds", "Kelvin",
				"Ampere", "Candela", "None" },
		{ "None", "Inch",       "Slug", "Seconds", "Fahrenheit",
				"Ampere", "Candela", "None" },
		{ "None", "Degrees",    "Slug", "Seconds", "Fahrenheit",
				"Ampere", "Candela", "None" },
	};

	unsigned int i;
	unsigned int sys;
	int earlier_unit = 0;

	/* First nibble tells us which system we're in. */
	sys = data & 0xf;
	data >>= 4;

	if (sys > 4) {
		if (sys == 0xf)
			printf("System: Vendor defined, Unit: (unknown)\n");
		else
			printf("System: Reserved, Unit: (unknown)\n");
		return;
	} else {
		printf("System: %s, Unit: ", systems[sys]);
	}
	for (i = 1 ; i < len * 2 ; i++) {
		char nibble = data & 0xf;
		data >>= 4;
		if (nibble != 0) {
			if (earlier_unit++ > 0)
				printf("*");
			printf("%s", units[sys][i]);
			if (nibble != 1) {
				/* This is a _signed_ nibble(!) */

				int val = nibble & 0x7;
				if (nibble & 0x08)
					val = -((0x7 & ~val) + 1);
				printf("^%d", val);
			}
		}
	}
	if (earlier_unit == 0)
		printf("(None)");
	printf("\n");
}

/* ---------------------------------------------------------------------- */

/*
 * Audio Class descriptor dump
 */

struct bmcontrol {
	const char *name;
	unsigned int bit;
};

static const struct bmcontrol uac2_interface_header_bmcontrols[] = {
	{ "Latency control",	0 },
	{ NULL }
};

static const struct bmcontrol uac_fu_bmcontrols[] = {
	{ "Mute",		0 },
	{ "Volume",		1 },
	{ "Bass",		2 },
	{ "Mid",		3 },
	{ "Treble",		4 },
	{ "Graphic Equalizer",	5 },
	{ "Automatic Gain",	6 },
	{ "Delay",		7 },
	{ "Bass Boost",		8 },
	{ "Loudness",		9 },
	{ "Input gain",		10 },
	{ "Input gain pad",	11 },
	{ "Phase inverter",	12 },
	{ NULL }
};

static const struct bmcontrol uac2_input_term_bmcontrols[] = {
	{ "Copy Protect",	0 },
	{ "Connector",		1 },
	{ "Overload",		2 },
	{ "Cluster",		3 },
	{ "Underflow",		4 },
	{ "Overflow",		5 },
	{ NULL }
};

static const struct bmcontrol uac2_output_term_bmcontrols[] = {
	{ "Copy Protect",	0 },
	{ "Connector",		1 },
	{ "Overload",		2 },
	{ "Underflow",		3 },
	{ "Overflow",		4 },
	{ NULL }
};

static const struct bmcontrol uac2_mixer_unit_bmcontrols[] = {
	{ "Cluster",		0 },
	{ "Underflow",		1 },
	{ "Overflow",		2 },
	{ NULL }
};

static const struct bmcontrol uac2_extension_unit_bmcontrols[] = {
	{ "Enable",		0 },
	{ "Cluster",		1 },
	{ "Underflow",		2 },
	{ "Overflow",		3 },
	{ NULL }
};

static const struct bmcontrol uac2_clock_source_bmcontrols[] = {
	{ "Clock Frequency",	0 },
	{ "Clock Validity",	1 },
	{ NULL }
};

static const struct bmcontrol uac2_clock_selector_bmcontrols[] = {
	{ "Clock Selector",	0 },
	{ NULL }
};

static const struct bmcontrol uac2_clock_multiplier_bmcontrols[] = {
	{ "Clock Numerator",	0 },
	{ "Clock Denominator",	1 },
	{ NULL }
};

static const struct bmcontrol uac2_selector_bmcontrols[] = {
	{ "Selector",	0 },
	{ NULL }
};

static void dump_audio_bmcontrols(const char *prefix, int bmcontrols, const struct bmcontrol *list, int protocol)
{
	while (list->name) {
		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			if (bmcontrols & (1 << list->bit))
				printf("%s%s Control\n", prefix, list->name);

			break;

		case USB_AUDIO_CLASS_2: {
			const char * const ctrl_type[] = { "read-only", "ILLEGAL (0b10)", "read/write" };
			int ctrl = (bmcontrols >> (list->bit * 2)) & 0x3;

			if (ctrl)
				printf("%s%s Control (%s)\n", prefix, list->name, ctrl_type[ctrl-1]);

			break;
		}

		} /* switch */

		list++;
	}
}

static const char * const chconfig_uac2[] = {
	"Front Left (FL)", "Front Right (FR)", "Front Center (FC)", "Low Frequency Effects (LFE)",
	"Back Left (BL)", "Back Right (BR)", "Front Left of Center (FLC)", "Front Right of Center (FRC)", "Back Center (BC)",
	"Side Left (SL)", "Side Right (SR)",
	"Top Center (TC)", "Top Front Left (TFL)", "Top Front Center (TFC)", "Top Front Right (TFR)", "Top Back Left (TBL)",
	"Top Back Center (TBC)", "Top Back Right (TBR)", "Top Front Left of Center (TFLC)", "Top Front Right of Center (TFRC)",
	"Left Low Frequency Effects (LLFE)", "Right Low Frequency Effects (RLFE)",
	"Top Side Left (TSL)", "Top Side Right (TSR)", "Bottom Center (BC)",
	"Back Left of Center (BLC)", "Back Right of Center (BRC)"
};

static void dump_audiocontrol_interface(libusb_device_handle *dev, const unsigned char *buf, int protocol)
{
	static const char * const chconfig[] = {
		"Left Front (L)", "Right Front (R)", "Center Front (C)", "Low Freqency Enhancement (LFE)",
		"Left Surround (LS)", "Right Surround (RS)", "Left of Center (LC)", "Right of Center (RC)",
		"Surround (S)", "Side Left (SL)", "Side Right (SR)", "Top (T)"
	};
	static const char * const clock_source_attrs[] = {
		"External", "Internal fixed", "Internal variable", "Internal programmable"
	};
	unsigned int i, chcfg, j, k, N, termt, subtype;
	char *chnames = NULL, *term = NULL, termts[128];

	if (buf[1] != USB_DT_CS_INTERFACE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 3)
		printf("      Warning: Descriptor too short\n");
	printf("      AudioControl Interface Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bDescriptorSubtype  %5u ",
	       buf[0], buf[1], buf[2]);

	/*
	 * This is an utter mess - UAC2 defines some bDescriptorSubtype differently, so we have to do some ugly remapping here:
	 *
	 * bDescriptorSubtype		UAC1			UAC2
	 * ------------------------------------------------------------------------
	 * 0x07				PROCESSING_UNIT		EFFECT_UNIT
	 * 0x08				EXTENSION_UNIT		PROCESSING_UNIT
	 * 0x09				-			EXTENSION_UNIT
	 *
	 */

	if (protocol == USB_AUDIO_CLASS_2)
		switch(buf[2]) {
		case 0x07: subtype = 0xf0; break; /* effect unit */
		case 0x08: subtype = 0x07; break; /* processing unit */
		case 0x09: subtype = 0x08; break; /* extension unit */
		default: subtype = buf[2]; break; /* everything else is identical */
		}
	else
		subtype = buf[2];

	switch (subtype) {
	case 0x01:  /* HEADER */
		printf("(HEADER)\n");
		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			if (buf[0] < 8+buf[7])
				printf("      Warning: Descriptor too short\n");
			printf("        bcdADC              %2x.%02x\n"
			       "        wTotalLength        %5u\n"
			       "        bInCollection       %5u\n",
			       buf[4], buf[3], buf[5] | (buf[6] << 8), buf[7]);
			for (i = 0; i < buf[7]; i++)
				printf("        baInterfaceNr(%2u)   %5u\n", i, buf[8+i]);
			dump_junk(buf, "        ", 8+buf[7]);
			break;
		case USB_AUDIO_CLASS_2:
			if (buf[0] < 9)
				printf("      Warning: Descriptor too short\n");
			printf("        bcdADC              %2x.%02x\n"
			       "        bCategory           %5u\n"
			       "        wTotalLength        %5u\n"
			       "        bmControl            0x%02x\n",
			       buf[4], buf[3], buf[5], buf[6] | (buf[7] << 8), buf[8]);
			dump_audio_bmcontrols("          ", buf[8], uac2_interface_header_bmcontrols, protocol);
			break;
		}
		break;

	case 0x02:  /* INPUT_TERMINAL */
		printf("(INPUT_TERMINAL)\n");
		termt = buf[4] | (buf[5] << 8);
		get_audioterminal_string(termts, sizeof(termts), termt);

		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			chnames = get_dev_string(dev, buf[10]);
			term = get_dev_string(dev, buf[11]);
			if (buf[0] < 12)
				printf("      Warning: Descriptor too short\n");
			chcfg = buf[8] | (buf[9] << 8);
			printf("        bTerminalID         %5u\n"
			       "        wTerminalType      0x%04x %s\n"
			       "        bAssocTerminal      %5u\n"
			       "        bNrChannels         %5u\n"
			       "        wChannelConfig     0x%04x\n",
			       buf[3], termt, termts, buf[6], buf[7], chcfg);
			for (i = 0; i < 12; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig[i]);
			printf("        iChannelNames       %5u %s\n"
			       "        iTerminal           %5u %s\n",
			       buf[10], chnames, buf[11], term);
			dump_junk(buf, "        ", 12);
			break;
		case USB_AUDIO_CLASS_2:
			chnames = get_dev_string(dev, buf[13]);
			term = get_dev_string(dev, buf[16]);
			if (buf[0] < 17)
				printf("      Warning: Descriptor too short\n");
			chcfg = buf[9] | (buf[10] << 8) | (buf[11] << 16) | (buf[12] << 24);
			printf("        bTerminalID         %5u\n"
			       "        wTerminalType      0x%04x %s\n"
			       "        bAssocTerminal      %5u\n"
			       "        bCSourceID          %5d\n"
			       "        bNrChannels         %5u\n"
			       "        bmChannelConfig   0x%08x\n",
			       buf[3], termt, termts, buf[6], buf[7], buf[8], chcfg);
			for (i = 0; i < 26; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig_uac2[i]);
			printf("        bmControls    0x%04x\n", buf[14] | (buf[15] << 8));
			dump_audio_bmcontrols("          ", buf[14] | (buf[15] << 8), uac2_input_term_bmcontrols, protocol);
			printf("        iChannelNames       %5u %s\n"
			       "        iTerminal           %5u %s\n",
			       buf[13], chnames, buf[16], term);
			dump_junk(buf, "        ", 17);
			break;
		} /* switch (protocol) */

		break;

	case 0x03:  /* OUTPUT_TERMINAL */
		printf("(OUTPUT_TERMINAL)\n");
		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			term = get_dev_string(dev, buf[8]);
			termt = buf[4] | (buf[5] << 8);
			get_audioterminal_string(termts, sizeof(termts), termt);
			if (buf[0] < 9)
				printf("      Warning: Descriptor too short\n");
			printf("        bTerminalID         %5u\n"
			       "        wTerminalType      0x%04x %s\n"
			       "        bAssocTerminal      %5u\n"
			       "        bSourceID           %5u\n"
			       "        iTerminal           %5u %s\n",
			       buf[3], termt, termts, buf[6], buf[7], buf[8], term);
			dump_junk(buf, "        ", 9);
			break;
		case USB_AUDIO_CLASS_2:
			term = get_dev_string(dev, buf[11]);
			termt = buf[4] | (buf[5] << 8);
			get_audioterminal_string(termts, sizeof(termts), termt);
			if (buf[0] < 12)
				printf("      Warning: Descriptor too short\n");
			printf("        bTerminalID         %5u\n"
			       "        wTerminalType      0x%04x %s\n"
			       "        bAssocTerminal      %5u\n"
			       "        bSourceID           %5u\n"
			       "        bCSourceID          %5u\n"
			       "        bmControls         0x%04x\n",
			       buf[3], termt, termts, buf[6], buf[7], buf[8], buf[9] | (buf[10] << 8));
			dump_audio_bmcontrols("          ", buf[9] | (buf[10] << 8), uac2_output_term_bmcontrols, protocol);
			printf("        iTerminal           %5u %s\n", buf[11], term);
			dump_junk(buf, "        ", 12);
			break;
		} /* switch (protocol) */

		break;

	case 0x04:  /* MIXER_UNIT */
		printf("(MIXER_UNIT)\n");

		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			j = buf[4];
			k = buf[j+5];
			if (j == 0 || k == 0) {
				printf("      Warning: mixer with %5u input and %5u output channels.\n", j, k);
				N = 0;
			} else {
				N = 1+(j*k-1)/8;
			}
			chnames = get_dev_string(dev, buf[8+j]);
			term = get_dev_string(dev, buf[9+j+N]);
			if (buf[0] < 10+j+N)
				printf("      Warning: Descriptor too short\n");
			chcfg = buf[6+j] | (buf[7+j] << 8);
			printf("        bUnitID             %5u\n"
			       "        bNrInPins           %5u\n",
			       buf[3], buf[4]);
			for (i = 0; i < j; i++)
				printf("        baSourceID(%2u)      %5u\n", i, buf[5+i]);
			printf("        bNrChannels         %5u\n"
			       "        wChannelConfig     0x%04x\n",
			       buf[5+j], chcfg);
			for (i = 0; i < 12; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig[i]);
			printf("        iChannelNames       %5u %s\n",
			       buf[8+j], chnames);
			for (i = 0; i < N; i++)
				printf("        bmControls         0x%02x\n", buf[9+j+i]);
			printf("        iMixer              %5u %s\n", buf[9+j+N], term);
			dump_junk(buf, "        ", 10+j+N);
			break;

		case USB_AUDIO_CLASS_2:
			j = buf[4];
			k = buf[0] - 13 - j;
			chnames = get_dev_string(dev, buf[10+j]);
			term = get_dev_string(dev, buf[12+j+k]);
			chcfg =  buf[6+j] | (buf[7+j] << 8) | (buf[8+j] << 16) | (buf[9+j] << 24);

			printf("        bUnitID             %5u\n"
			       "        bNrPins             %5u\n",
			       buf[3], buf[4]);
			for (i = 0; i < j; i++)
				printf("        baSourceID(%2u)      %5u\n", i, buf[5+i]);
			printf("        bNrChannels         %5u\n"
			       "        bmChannelConfig    0x%08x\n", buf[5+j], chcfg);
			for (i = 0; i < 26; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig_uac2[i]);
			printf("        iChannelNames       %5u %s\n", buf[10+j], chnames);

			N = 0;
			for (i = 0; i < k; i++)
				N |= buf[11+j+i] << (i * 8);

			dump_bytes(buf+11+j, k);

			printf("        bmControls         %02x\n", buf[11+j+k]);
			dump_audio_bmcontrols("          ", buf[11+j+k], uac2_mixer_unit_bmcontrols, protocol);

			printf("        iMixer             %5u %s\n", buf[12+j+k], term);
			dump_junk(buf, "        ", 13+j+k);
			break;
		} /* switch (protocol) */
		break;

	case 0x05:  /* SELECTOR_UNIT */
		printf("(SELECTOR_UNIT)\n");
		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			if (buf[0] < 6+buf[4])
				printf("      Warning: Descriptor too short\n");
			term = get_dev_string(dev, buf[5+buf[4]]);

			printf("        bUnitID             %5u\n"
			       "        bNrInPins           %5u\n",
			       buf[3], buf[4]);
			for (i = 0; i < buf[4]; i++)
				printf("        baSource(%2u)        %5u\n", i, buf[5+i]);
			printf("        iSelector           %5u %s\n",
			       buf[5+buf[4]], term);
			dump_junk(buf, "        ", 6+buf[4]);
			break;
		case USB_AUDIO_CLASS_2:
			if (buf[0] < 7+buf[4])
				printf("      Warning: Descriptor too short\n");
			term = get_dev_string(dev, buf[6+buf[4]]);

			printf("        bUnitID             %5u\n"
			       "        bNrInPins           %5u\n",
			       buf[3], buf[4]);
			for (i = 0; i < buf[4]; i++)
				printf("        baSource(%2u)        %5u\n", i, buf[5+i]);
			printf("        bmControls           0x%02x\n", buf[5+buf[4]]);
			dump_audio_bmcontrols("          ", buf[5+buf[4]], uac2_selector_bmcontrols, protocol);
			printf("        iSelector           %5u %s\n",
			       buf[6+buf[4]], term);
			dump_junk(buf, "        ", 7+buf[4]);
			break;
		} /* switch (protocol) */

		break;

	case 0x06:  /* FEATURE_UNIT */
		printf("(FEATURE_UNIT)\n");

		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			j = buf[5];
			if (!j)
				j = 1;
			k = (buf[0] - 7) / j;
			if (buf[0] < 7+buf[5]*k)
				printf("      Warning: Descriptor too short\n");
			term = get_dev_string(dev, buf[6+buf[5]*k]);
			printf("        bUnitID             %5u\n"
			       "        bSourceID           %5u\n"
			       "        bControlSize        %5u\n",
			       buf[3], buf[4], buf[5]);
			for (i = 0; i < k; i++) {
				chcfg = buf[6+buf[5]*i];
				if (buf[5] > 1)
					chcfg |= (buf[7+buf[5]*i] << 8);
				for (j = 0; j < buf[5]; j++)
					printf("        bmaControls(%2u)      0x%02x\n", i, buf[6+buf[5]*i+j]);

				dump_audio_bmcontrols("          ", chcfg, uac_fu_bmcontrols, protocol);
			}
			printf("        iFeature            %5u %s\n", buf[6+buf[5]*k], term);
			dump_junk(buf, "        ", 7+buf[5]*k);
			break;
		case USB_AUDIO_CLASS_2:
			if (buf[0] < 10)
				printf("      Warning: Descriptor too short\n");
			k = (buf[0] - 6) / 4;
			printf("        bUnitID             %5u\n"
			       "        bSourceID           %5u\n",
			       buf[3], buf[4]);
			for (i = 0; i < k; i++) {
				chcfg = buf[5+(4*i)] |
					buf[6+(4*i)] << 8 |
					buf[7+(4*i)] << 16 |
					buf[8+(4*i)] << 24;
				printf("        bmaControls(%2u)      0x%08x\n", i, chcfg);
				dump_audio_bmcontrols("          ", chcfg, uac_fu_bmcontrols, protocol);
			}
			term = get_dev_string(dev, buf[5+k*4]);
			printf("        iFeature            %5u %s\n", buf[5+(k*4)], term);
			dump_junk(buf, "        ", 6+(k*4));
			break;
		} /* switch (protocol) */

		break;

	case 0x07:  /* PROCESSING_UNIT */
		printf("(PROCESSING_UNIT)\n");

		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			j = buf[6];
			k = buf[11+j];
			chnames = get_dev_string(dev, buf[10+j]);
			term = get_dev_string(dev, buf[12+j+k]);
			chcfg = buf[8+j] | (buf[9+j] << 8);
			if (buf[0] < 13+j+k)
				printf("      Warning: Descriptor too short\n");
			printf("        bUnitID             %5u\n"
			       "        wProcessType        %5u\n"
			       "        bNrPins             %5u\n",
			       buf[3], buf[4] | (buf[5] << 8), buf[6]);
			for (i = 0; i < j; i++)
				printf("        baSourceID(%2u)      %5u\n", i, buf[7+i]);
			printf("        bNrChannels         %5u\n"
			       "        wChannelConfig     0x%04x\n", buf[7+j], chcfg);
			for (i = 0; i < 12; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig[i]);
			printf("        iChannelNames       %5u %s\n"
			       "        bControlSize        %5u\n", buf[10+j], chnames, buf[11+j]);
			for (i = 0; i < k; i++)
				printf("        bmControls(%2u)       0x%02x\n", i, buf[12+j+i]);
			if (buf[12+j] & 1)
				printf("          Enable Processing\n");
			printf("        iProcessing         %5u %s\n"
			       "        Process-Specific    ", buf[12+j+k], term);
			dump_bytes(buf+(13+j+k), buf[0]-(13+j+k));
			break;
		case USB_AUDIO_CLASS_2:
			j = buf[6];
			k = buf[0] - 17 - j;
			chnames = get_dev_string(dev, buf[12+j]);
			term = get_dev_string(dev, buf[15+j+k]);
			chcfg =  buf[8+j] |
				(buf[9+j] << 8) |
				(buf[10+j] << 16) |
				(buf[11+j] << 24);

			printf("        bUnitID             %5u\n"
			       "        wProcessType        %5u\n"
			       "        bNrPins             %5u\n",
			       buf[3], buf[4] | (buf[5] << 8), buf[6]);
			for (i = 0; i < j; i++)
				printf("        baSourceID(%2u)      %5u\n", i, buf[5+i]);
			printf("        bNrChannels         %5u\n"
			       "        bmChannelConfig    0x%08x\n", buf[7+j], chcfg);
			for (i = 0; i < 26; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig_uac2[i]);
			printf("        iChannelNames       %5u %s\n"
			       "        bmControls        0x%04x\n", buf[12+j], chnames, buf[13+j] | (buf[14+j] << 8));
			if (buf[12+j] & 1)
				printf("          Enable Processing\n");
			printf("        iProcessing         %5u %s\n"
			       "        Process-Specific    ", buf[15+j], term);
			dump_bytes(buf+(16+j), k);
			break;
		} /* switch (protocol) */

		break;

	case 0x08:  /* EXTENSION_UNIT */
		printf("(EXTENSION_UNIT)\n");

		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			j = buf[6];
			k = buf[11+j];
			chnames = get_dev_string(dev, buf[10+j]);
			term = get_dev_string(dev, buf[12+j+k]);
			chcfg = buf[8+j] | (buf[9+j] << 8);
			if (buf[0] < 13+j+k)
				printf("      Warning: Descriptor too short\n");
			printf("        bUnitID             %5u\n"
			       "        wExtensionCode      %5u\n"
			       "        bNrPins             %5u\n",
			       buf[3], buf[4] | (buf[5] << 8), buf[6]);
			for (i = 0; i < j; i++)
				printf("        baSourceID(%2u)      %5u\n", i, buf[7+i]);
			printf("        bNrChannels         %5u\n"
			       "        wChannelConfig      %5u\n", buf[7+j], chcfg);
			for (i = 0; i < 12; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig[i]);
			printf("        iChannelNames       %5u %s\n"
			       "        bControlSize        %5u\n", buf[10+j], chnames, buf[11+j]);
			for (i = 0; i < k; i++)
				printf("        bmControls(%2u)       0x%02x\n", i, buf[12+j+i]);
			if (buf[12+j] & 1)
				printf("          Enable Processing\n");
			printf("        iExtension          %5u %s\n",
			       buf[12+j+k], term);
			dump_junk(buf, "        ", 13+j+k);
			break;
		case USB_AUDIO_CLASS_2:
			j = buf[6];
			chnames = get_dev_string(dev, buf[13+j]);
			term = get_dev_string(dev, buf[15+j]);
			chcfg = buf[9+j] | (buf[10+j] << 8) | (buf[11+j] << 16) | (buf[12+j] << 24);
			if (buf[0] < 16+j)
				printf("      Warning: Descriptor too short\n");
			printf("        bUnitID             %5u\n"
			       "        wExtensionCode      %5u\n"
			       "        bNrPins             %5u\n",
			       buf[3], buf[4] | (buf[5] << 8), buf[6]);
			for (i = 0; i < j; i++)
				printf("        baSourceID(%2u)      %5u\n", i, buf[7+i]);
			printf("        bNrChannels         %5u\n"
			       "        wChannelConfig      %5u\n", buf[7+j], chcfg);
			for (i = 0; i < 12; i++)
				if ((chcfg >> i) & 1)
					printf("          %s\n", chconfig[i]);
			printf("        iChannelNames       %5u %s\n"
			       "        bmControls        0x%02x\n", buf[13+j], chnames, buf[14+j]);
			dump_audio_bmcontrols("          ", buf[14+j], uac2_extension_unit_bmcontrols, protocol);

			printf("        iExtension          %5u %s\n",
			       buf[15+j], term);
			dump_junk(buf, "        ", 16+j);
			break;
		} /* switch (protocol) */

		break;

	case 0x0a:  /* CLOCK_SOURCE */
		printf ("(CLOCK_SOURCE)\n");
		if (protocol != USB_AUDIO_CLASS_2)
			printf("      Warning: CLOCK_SOURCE descriptors are illegal for UAC1\n");

		if (buf[0] < 8)
			printf("      Warning: Descriptor too short\n");

		printf("        bClockID            %5u\n"
		       "        bmAttributes         0x%02x %s Clock %s\n",
		       buf[3], buf[4], clock_source_attrs[buf[4] & 3],
		       (buf[4] & 4) ? "(synced to SOF)" : "");

		printf("        bmControls           0x%02x\n", buf[5]);
		dump_audio_bmcontrols("          ", buf[5], uac2_clock_source_bmcontrols, protocol);

		term = get_dev_string(dev, buf[7]);
		printf("        bAssocTerminal      %5u\n", buf[6]);
		printf("        iClockSource        %5u %s\n", buf[7], term);
		dump_junk(buf, "        ", 8);
		break;

	case 0x0b:  /* CLOCK_SELECTOR */
		printf("(CLOCK_SELECTOR)\n");
		if (protocol != USB_AUDIO_CLASS_2)
			printf("      Warning: CLOCK_SELECTOR descriptors are illegal for UAC1\n");

		if (buf[0] < 7+buf[4])
			printf("      Warning: Descriptor too short\n");
		term = get_dev_string(dev, buf[6+buf[4]]);

		printf("        bUnitID             %5u\n"
		       "        bNrInPins           %5u\n",
		       buf[3], buf[4]);
		for (i = 0; i < buf[4]; i++)
			printf("        baCSourceID(%2u)     %5u\n", i, buf[5+i]);
		printf("        bmControls           0x%02x\n", buf[5+buf[4]]);
		dump_audio_bmcontrols("          ", buf[5+buf[4]], uac2_clock_selector_bmcontrols, protocol);

		printf("        iClockSelector      %5u %s\n",
		       buf[6+buf[4]], term);
		dump_junk(buf, "        ", 7+buf[4]);
		break;

	case 0x0c:  /* CLOCK_MULTIPLIER */
		printf("(CLOCK_MULTIPLIER)\n");
		if (protocol != USB_AUDIO_CLASS_2)
			printf("      Warning: CLOCK_MULTIPLIER descriptors are illegal for UAC1\n");

		if (buf[0] < 7)
			printf("      Warning: Descriptor too short\n");

		printf("        bClockID            %5u\n"
		       "        bCSourceID          %5u\n",
		       buf[3], buf[4]);

		printf("        bmControls           0x%02x\n", buf[5]);
		dump_audio_bmcontrols("          ", buf[5], uac2_clock_multiplier_bmcontrols, protocol);

		term = get_dev_string(dev, buf[6]);
		printf("        iClockMultiplier    %5u %s\n", buf[6], term);
		dump_junk(buf, "        ", 7);
		break;

	case 0x0d:  /* SAMPLE_RATE_CONVERTER_UNIT */
		printf("(SAMPLE_RATE_CONVERTER_UNIT)\n");
		if (protocol != USB_AUDIO_CLASS_2)
			printf("      Warning: SAMPLE_RATE_CONVERTER_UNIT descriptors are illegal for UAC1\n");

		if (buf[0] < 8)
			printf("      Warning: Descriptor too short\n");

		term = get_dev_string(dev, buf[7]);
		printf("        bUnitID             %5u\n"
		       "        bSourceID           %5u\n"
		       "        bCSourceInID        %5u\n"
		       "        bCSourceOutID       %5u\n"
		       "        iSRC                %5u %s\n",
		       buf[3], buf[4], buf[5], buf[6], buf[7], term);
		dump_junk(buf, "        ", 8);
		break;

	case 0xf0:  /* EFFECT_UNIT - the real value is 0x07, see above for the reason for remapping */
		printf("(EFFECT_UNIT)\n");

		if (buf[0] < 16)
			printf("      Warning: Descriptor too short\n");
		k = (buf[0] - 16) / 4;
		term = get_dev_string(dev, buf[15+k*4]);
		printf("        bUnitID             %5u\n"
		       "        wEffectType         %5u\n"
		       "        bSourceID           %5u\n",
		       buf[3], buf[4] | (buf[5] << 8), buf[6]);
		for (i = 0; i < k; i++) {
			chcfg = buf[7+(4*i)] |
				buf[8+(4*i)] << 8 |
				buf[9+(4*i)] << 16 |
				buf[10+(4*i)] << 24;
			printf("        bmaControls(%2u)      0x%08x\n", i, chcfg);
			/* TODO: parse effect-specific controls */
		}
		printf("        iEffect             %5u %s\n", buf[15+(k*4)], term);
		dump_junk(buf, "        ", 16+(k*4));
		break;

	default:
		printf("(unknown)\n"
		       "        Invalid desc subtype:");
		dump_bytes(buf+3, buf[0]-3);
		break;
	}

	free(chnames);
	free(term);
}

static const struct bmcontrol uac2_as_interface_bmcontrols[] = {
	{ "Active Alternate Setting",	0 },
	{ "Valid Alternate Setting",	1 },
	{ NULL }
};

static void dump_audiostreaming_interface(libusb_device_handle *dev, const unsigned char *buf, int protocol)
{
	static const char * const fmtItag[] = {
		"TYPE_I_UNDEFINED", "PCM", "PCM8", "IEEE_FLOAT", "ALAW", "MULAW" };
	static const char * const fmtIItag[] = { "TYPE_II_UNDEFINED", "MPEG", "AC-3" };
	static const char * const fmtIIItag[] = {
		"TYPE_III_UNDEFINED", "IEC1937_AC-3", "IEC1937_MPEG-1_Layer1",
		"IEC1937_MPEG-Layer2/3/NOEXT", "IEC1937_MPEG-2_EXT",
		"IEC1937_MPEG-2_Layer1_LS", "IEC1937_MPEG-2_Layer2/3_LS" };
	unsigned int i, j, fmttag;
	const char *fmtptr = "undefined";
	char *name = NULL;

	if (buf[1] != USB_DT_CS_INTERFACE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 3)
		printf("      Warning: Descriptor too short\n");
	printf("      AudioStreaming Interface Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bDescriptorSubtype  %5u ",
	       buf[0], buf[1], buf[2]);
	switch (buf[2]) {
	case 0x01: /* AS_GENERAL */
		printf("(AS_GENERAL)\n");

		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			if (buf[0] < 7)
				printf("      Warning: Descriptor too short\n");
			fmttag = buf[5] | (buf[6] << 8);
			if (fmttag <= 5)
				fmtptr = fmtItag[fmttag];
			else if (fmttag >= 0x1000 && fmttag <= 0x1002)
				fmtptr = fmtIItag[fmttag & 0xfff];
			else if (fmttag >= 0x2000 && fmttag <= 0x2006)
				fmtptr = fmtIIItag[fmttag & 0xfff];
			printf("        bTerminalLink       %5u\n"
			       "        bDelay              %5u frames\n"
			       "        wFormatTag          %5u %s\n",
			       buf[3], buf[4], fmttag, fmtptr);
			dump_junk(buf, "        ", 7);
			break;
		case USB_AUDIO_CLASS_2:
			if (buf[0] < 16)
				printf("      Warning: Descriptor too short\n");
			printf("        bTerminalLink       %5u\n"
			       "        bmControls           0x%02x\n",
			       buf[3], buf[4]);
			dump_audio_bmcontrols("          ", buf[4], uac2_as_interface_bmcontrols, protocol);

			printf("        bFormatType         %5u\n", buf[5]);
			fmttag = buf[6] | (buf[7] << 8) | (buf[8] << 16) | (buf[9] << 24);
			printf("        bmFormats         0x%08x\n", fmttag);
			for (i=0; i<5; i++)
				if ((fmttag >> i) & 1)
					printf("          %s\n", fmtItag[i+1]);

			j = buf[11] | (buf[12] << 8) | (buf[13] << 16) | (buf[14] << 24);
			printf("        bNrChannels         %5u\n"
			       "        bmChannelConfig   0x%08x\n",
			       buf[10], j);
			for (i = 0; i < 26; i++)
				if ((j >> i) & 1)
					printf("          %s\n", chconfig_uac2[i]);

			name = get_dev_string(dev, buf[15]);
			printf("        iChannelNames       %5u %s\n", buf[15], name);
			dump_junk(buf, "        ", 16);
			break;
		} /* switch (protocol) */

		break;

	case 0x02: /* FORMAT_TYPE */
		printf("(FORMAT_TYPE)\n");
		switch (protocol) {
		case USB_AUDIO_CLASS_1:
			if (buf[0] < 8)
				printf("      Warning: Descriptor too short\n");
			printf("        bFormatType         %5u ", buf[3]);
			switch (buf[3]) {
			case 0x01: /* FORMAT_TYPE_I */
				printf("(FORMAT_TYPE_I)\n");
				j = buf[7] ? (buf[7]*3+8) : 14;
				if (buf[0] < j)
					printf("      Warning: Descriptor too short\n");
				printf("        bNrChannels         %5u\n"
				       "        bSubframeSize       %5u\n"
				       "        bBitResolution      %5u\n"
				       "        bSamFreqType        %5u %s\n",
				       buf[4], buf[5], buf[6], buf[7], buf[7] ? "Discrete" : "Continuous");
				if (!buf[7])
					printf("        tLowerSamFreq     %7u\n"
					       "        tUpperSamFreq     %7u\n",
					       buf[8] | (buf[9] << 8) | (buf[10] << 16), buf[11] | (buf[12] << 8) | (buf[13] << 16));
				else
					for (i = 0; i < buf[7]; i++)
						printf("        tSamFreq[%2u]      %7u\n", i,
						       buf[8+3*i] | (buf[9+3*i] << 8) | (buf[10+3*i] << 16));
				dump_junk(buf, "        ", j);
				break;

			case 0x02: /* FORMAT_TYPE_II */
				printf("(FORMAT_TYPE_II)\n");
				j = buf[8] ? (buf[7]*3+9) : 15;
				if (buf[0] < j)
					printf("      Warning: Descriptor too short\n");
				printf("        wMaxBitRate         %5u\n"
				       "        wSamplesPerFrame    %5u\n"
				       "        bSamFreqType        %5u %s\n",
				       buf[4] | (buf[5] << 8), buf[6] | (buf[7] << 8), buf[8], buf[8] ? "Discrete" : "Continuous");
				if (!buf[8])
					printf("        tLowerSamFreq     %7u\n"
					       "        tUpperSamFreq     %7u\n",
					       buf[9] | (buf[10] << 8) | (buf[11] << 16), buf[12] | (buf[13] << 8) | (buf[14] << 16));
				else
					for (i = 0; i < buf[8]; i++)
						printf("        tSamFreq[%2u]      %7u\n", i,
						       buf[9+3*i] | (buf[10+3*i] << 8) | (buf[11+3*i] << 16));
				dump_junk(buf, "        ", j);
				break;

			case 0x03: /* FORMAT_TYPE_III */
				printf("(FORMAT_TYPE_III)\n");
				j = buf[7] ? (buf[7]*3+8) : 14;
				if (buf[0] < j)
					printf("      Warning: Descriptor too short\n");
				printf("        bNrChannels         %5u\n"
				       "        bSubframeSize       %5u\n"
				       "        bBitResolution      %5u\n"
				       "        bSamFreqType        %5u %s\n",
				       buf[4], buf[5], buf[6], buf[7], buf[7] ? "Discrete" : "Continuous");
				if (!buf[7])
					printf("        tLowerSamFreq     %7u\n"
					       "        tUpperSamFreq     %7u\n",
					       buf[8] | (buf[9] << 8) | (buf[10] << 16), buf[11] | (buf[12] << 8) | (buf[13] << 16));
				else
					for (i = 0; i < buf[7]; i++)
						printf("        tSamFreq[%2u]      %7u\n", i,
						       buf[8+3*i] | (buf[9+3*i] << 8) | (buf[10+3*i] << 16));
				dump_junk(buf, "        ", j);
				break;

			default:
				printf("(unknown)\n"
				       "        Invalid desc format type:");
				dump_bytes(buf+4, buf[0]-4);
			}

			break;

		case USB_AUDIO_CLASS_2:
			printf("        bFormatType         %5u ", buf[3]);
			switch (buf[3]) {
			case 0x01: /* FORMAT_TYPE_I */
				printf("(FORMAT_TYPE_I)\n");
				if (buf[0] < 6)
					printf("      Warning: Descriptor too short\n");
				printf("        bSubslotSize        %5u\n"
				       "        bBitResolution      %5u\n",
				       buf[4], buf[5]);
				dump_junk(buf, "        ", 6);
				break;

			case 0x02: /* FORMAT_TYPE_II */
				printf("(FORMAT_TYPE_II)\n");
				if (buf[0] < 8)
					printf("      Warning: Descriptor too short\n");
				printf("        wMaxBitRate         %5u\n"
				       "        wSlotsPerFrame      %5u\n",
				       buf[4] | (buf[5] << 8),
				       buf[6] | (buf[7] << 8));
				dump_junk(buf, "        ", 8);
				break;

			case 0x03: /* FORMAT_TYPE_III */
				printf("(FORMAT_TYPE_III)\n");
				if (buf[0] < 6)
					printf("      Warning: Descriptor too short\n");
				printf("        bSubslotSize        %5u\n"
				       "        bBitResolution      %5u\n",
				       buf[4], buf[5]);
				dump_junk(buf, "        ", 6);
				break;

			case 0x04: /* FORMAT_TYPE_IV */
				printf("(FORMAT_TYPE_IV)\n");
				if (buf[0] < 4)
					printf("      Warning: Descriptor too short\n");
				printf("        bFormatType         %5u\n", buf[3]);
				dump_junk(buf, "        ", 4);
				break;

			default:
				printf("(unknown)\n"
				       "        Invalid desc format type:");
				dump_bytes(buf+4, buf[0]-4);
			}

			break;
		} /* switch (protocol) */

		break;

	case 0x03: /* FORMAT_SPECIFIC */
		printf("(FORMAT_SPECIFIC)\n");
		if (buf[0] < 5)
			printf("      Warning: Descriptor too short\n");
		fmttag = buf[3] | (buf[4] << 8);
		if (fmttag <= 5)
			fmtptr = fmtItag[fmttag];
		else if (fmttag >= 0x1000 && fmttag <= 0x1002)
			fmtptr = fmtIItag[fmttag & 0xfff];
		else if (fmttag >= 0x2000 && fmttag <= 0x2006)
			fmtptr = fmtIIItag[fmttag & 0xfff];
		printf("        wFormatTag          %5u %s\n", fmttag, fmtptr);
		switch (fmttag) {
		case 0x1001: /* MPEG */
			if (buf[0] < 8)
				printf("      Warning: Descriptor too short\n");
			printf("        bmMPEGCapabilities 0x%04x\n",
			       buf[5] | (buf[6] << 8));
			if (buf[5] & 0x01)
				printf("          Layer I\n");
			if (buf[5] & 0x02)
				printf("          Layer II\n");
			if (buf[5] & 0x04)
				printf("          Layer III\n");
			if (buf[5] & 0x08)
				printf("          MPEG-1 only\n");
			if (buf[5] & 0x10)
				printf("          MPEG-1 dual-channel\n");
			if (buf[5] & 0x20)
				printf("          MPEG-2 second stereo\n");
			if (buf[5] & 0x40)
				printf("          MPEG-2 7.1 channel augmentation\n");
			if (buf[5] & 0x80)
				printf("          Adaptive multi-channel prediction\n");
			printf("          MPEG-2 multilingual support: ");
			switch (buf[6] & 3) {
			case 0:
				printf("Not supported\n");
				break;

			case 1:
				printf("Supported at Fs\n");
				break;

			case 2:
				printf("Reserved\n");
				break;

			default:
				printf("Supported at Fs and 1/2Fs\n");
				break;
			}
			printf("        bmMPEGFeatures       0x%02x\n", buf[7]);
			printf("          Internal Dynamic Range Control: ");
			switch ((buf[7] >> 4) & 3) {
			case 0:
				printf("not supported\n");
				break;

			case 1:
				printf("supported but not scalable\n");
				break;

			case 2:
				printf("scalable, common boost and cut scaling value\n");
				break;

			default:
				printf("scalable, separate boost and cut scaling value\n");
				break;
			}
			dump_junk(buf, "        ", 8);
			break;

		case 0x1002: /* AC-3 */
			if (buf[0] < 10)
				printf("      Warning: Descriptor too short\n");
			printf("        bmBSID         0x%08x\n"
			       "        bmAC3Features        0x%02x\n",
			       buf[5] | (buf[6] << 8) | (buf[7] << 16) | (buf[8] << 24), buf[9]);
			if (buf[9] & 0x01)
				printf("          RF mode\n");
			if (buf[9] & 0x02)
				printf("          Line mode\n");
			if (buf[9] & 0x04)
				printf("          Custom0 mode\n");
			if (buf[9] & 0x08)
				printf("          Custom1 mode\n");
			printf("          Internal Dynamic Range Control: ");
			switch ((buf[9] >> 4) & 3) {
			case 0:
				printf("not supported\n");
				break;

			case 1:
				printf("supported but not scalable\n");
				break;

			case 2:
				printf("scalable, common boost and cut scaling value\n");
				break;

			default:
				printf("scalable, separate boost and cut scaling value\n");
				break;
			}
			dump_junk(buf, "        ", 8);
			break;

		default:
			printf("(unknown)\n"
			       "        Invalid desc format type:");
			dump_bytes(buf+4, buf[0]-4);
		}
		break;

	default:
		printf("        Invalid desc subtype:");
		dump_bytes(buf+3, buf[0]-3);
		break;
	}

	free(name);
}

static const struct bmcontrol uac2_audio_endpoint_bmcontrols[] = {
	{ "Pitch",		0 },
	{ "Data Overrun",	1 },
	{ "Data Underrun",	2 },
	{ NULL }
};

static void dump_audiostreaming_endpoint(const unsigned char *buf, int protocol)
{
	static const char * const lockdelunits[] = { "Undefined", "Milliseconds", "Decoded PCM samples", "Reserved" };
	unsigned int lckdelidx;

	if (buf[1] != USB_DT_CS_ENDPOINT)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < ((protocol == USB_AUDIO_CLASS_1) ? 7 : 8))
		printf("      Warning: Descriptor too short\n");
	printf("        AudioControl Endpoint Descriptor:\n"
	       "          bLength             %5u\n"
	       "          bDescriptorType     %5u\n"
	       "          bDescriptorSubtype  %5u (%s)\n"
	       "          bmAttributes         0x%02x\n",
	       buf[0], buf[1], buf[2], buf[2] == 1 ? "EP_GENERAL" : "invalid", buf[3]);

	switch (protocol) {
	case USB_AUDIO_CLASS_1:
		if (buf[3] & 1)
			printf("            Sampling Frequency\n");
		if (buf[3] & 2)
			printf("            Pitch\n");
		if (buf[3] & 128)
			printf("            MaxPacketsOnly\n");
		lckdelidx = buf[4];
		if (lckdelidx > 3)
			lckdelidx = 3;
		printf("          bLockDelayUnits     %5u %s\n"
		       "          wLockDelay          %5u %s\n",
		       buf[4], lockdelunits[lckdelidx], buf[5] | (buf[6] << 8), lockdelunits[lckdelidx]);
		dump_junk(buf, "        ", 7);
		break;

	case USB_AUDIO_CLASS_2:
		if (buf[3] & 128)
			printf("            MaxPacketsOnly\n");

		printf("          bmControls           0x%02x\n", buf[4]);
		dump_audio_bmcontrols("          ", buf[4], uac2_audio_endpoint_bmcontrols, protocol);

		lckdelidx = buf[5];
		if (lckdelidx > 3)
			lckdelidx = 3;
		printf("          bLockDelayUnits     %5u %s\n"
		       "          wLockDelay          %5u\n",
		       buf[5], lockdelunits[lckdelidx], buf[6] | (buf[7] << 8));
		dump_junk(buf, "        ", 8);
		break;
	} /* switch protocol */
}

static void dump_midistreaming_interface(libusb_device_handle *dev, const unsigned char *buf)
{
	static const char * const jacktypes[] = {"Undefined", "Embedded", "External"};
	char *jackstr = NULL;
	unsigned int j, tlength, capssize;
	unsigned long caps;

	if (buf[1] != USB_DT_CS_INTERFACE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 3)
		printf("      Warning: Descriptor too short\n");
	printf("      MIDIStreaming Interface Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bDescriptorSubtype  %5u ",
	       buf[0], buf[1], buf[2]);
	switch (buf[2]) {
	case 0x01:
		printf("(HEADER)\n");
		if (buf[0] < 7)
			printf("      Warning: Descriptor too short\n");
		tlength = buf[5] | (buf[6] << 8);
		printf("        bcdADC              %2x.%02x\n"
		       "        wTotalLength        %5u\n",
		       buf[4], buf[3], tlength);
		dump_junk(buf, "        ", 7);
		break;

	case 0x02:
		printf("(MIDI_IN_JACK)\n");
		if (buf[0] < 6)
			printf("      Warning: Descriptor too short\n");
		jackstr = get_dev_string(dev, buf[5]);
		printf("        bJackType           %5u %s\n"
		       "        bJackID             %5u\n"
		       "        iJack               %5u %s\n",
		       buf[3], buf[3] < 3 ? jacktypes[buf[3]] : "Invalid",
		       buf[4], buf[5], jackstr);
		dump_junk(buf, "        ", 6);
		break;

	case 0x03:
		printf("(MIDI_OUT_JACK)\n");
		if (buf[0] < 9)
			printf("      Warning: Descriptor too short\n");
		printf("        bJackType           %5u %s\n"
		       "        bJackID             %5u\n"
		       "        bNrInputPins        %5u\n",
		       buf[3], buf[3] < 3 ? jacktypes[buf[3]] : "Invalid",
		       buf[4], buf[5]);
		for (j = 0; j < buf[5]; j++) {
			printf("        baSourceID(%2u)      %5u\n"
			       "        BaSourcePin(%2u)     %5u\n",
			       j, buf[2*j+6], j, buf[2*j+7]);
		}
		j = 6+buf[5]*2; /* midi10.pdf says, incorrectly: 5+2*p */
		jackstr = get_dev_string(dev, buf[j]);
		printf("        iJack               %5u %s\n",
		       buf[j], jackstr);
		dump_junk(buf, "        ", j+1);
		break;

	case 0x04:
		printf("(ELEMENT)\n");
		if (buf[0] < 12)
			printf("      Warning: Descriptor too short\n");
		printf("        bElementID          %5u\n"
		       "        bNrInputPins        %5u\n",
		       buf[3], buf[4]);
		for (j = 0; j < buf[4]; j++) {
			printf("        baSourceID(%2u)      %5u\n"
			       "        BaSourcePin(%2u)     %5u\n",
			       j, buf[2*j+5], j, buf[2*j+6]);
		}
		j = 5+buf[4]*2;
		printf("        bNrOutputPins       %5u\n"
		       "        bInTerminalLink     %5u\n"
		       "        bOutTerminalLink    %5u\n"
		       "        bElCapsSize         %5u\n",
		       buf[j], buf[j+1], buf[j+2], buf[j+3]);
		capssize = buf[j+3];
		caps = 0;
		for (j = 0; j < capssize; j++)
			caps |= (buf[j+9+buf[4]*2] << (8*j));
		printf("        bmElementCaps  0x%08lx\n", caps);
		if (caps & 0x01)
			printf("          Undefined\n");
		if (caps & 0x02)
			printf("          MIDI Clock\n");
		if (caps & 0x04)
			printf("          MTC (MIDI Time Code)\n");
		if (caps & 0x08)
			printf("          MMC (MIDI Machine Control)\n");
		if (caps & 0x10)
			printf("          GM1 (General MIDI v.1)\n");
		if (caps & 0x20)
			printf("          GM2 (General MIDI v.2)\n");
		if (caps & 0x40)
			printf("          GS MIDI Extension\n");
		if (caps & 0x80)
			printf("          XG MIDI Extension\n");
		if (caps & 0x100)
			printf("          EFX\n");
		if (caps & 0x200)
			printf("          MIDI Patch Bay\n");
		if (caps & 0x400)
			printf("          DLS1 (Downloadable Sounds Level 1)\n");
		if (caps & 0x800)
			printf("          DLS2 (Downloadable Sounds Level 2)\n");
		j = 9+2*buf[4]+capssize;
		jackstr = get_dev_string(dev, buf[j]);
		printf("        iElement            %5u %s\n", buf[j], jackstr);
		dump_junk(buf, "        ", j+1);
		break;

	default:
		printf("\n        Invalid desc subtype: ");
		dump_bytes(buf+3, buf[0]-3);
		break;
	}

	free(jackstr);
}

static void dump_midistreaming_endpoint(const unsigned char *buf)
{
	unsigned int j;

	if (buf[1] != USB_DT_CS_ENDPOINT)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 5)
		printf("      Warning: Descriptor too short\n");
	printf("        MIDIStreaming Endpoint Descriptor:\n"
	       "          bLength             %5u\n"
	       "          bDescriptorType     %5u\n"
	       "          bDescriptorSubtype  %5u (%s)\n"
	       "          bNumEmbMIDIJack     %5u\n",
	       buf[0], buf[1], buf[2], buf[2] == 1 ? "GENERAL" : "Invalid", buf[3]);
	for (j = 0; j < buf[3]; j++)
		printf("          baAssocJackID(%2u)   %5u\n", j, buf[4+j]);
	dump_junk(buf, "          ", 4+buf[3]);
}

/*
 * Video Class descriptor dump
 */

static void dump_videocontrol_interface(libusb_device_handle *dev, const unsigned char *buf)
{
	static const char * const ctrlnames[] = {
		"Brightness", "Contrast", "Hue", "Saturation", "Sharpness", "Gamma",
		"White Balance Temperature", "White Balance Component", "Backlight Compensation",
		"Gain", "Power Line Frequency", "Hue, Auto", "White Balance Temperature, Auto",
		"White Balance Component, Auto", "Digital Multiplier", "Digital Multiplier Limit",
		"Analog Video Standard", "Analog Video Lock Status"
	};
	static const char * const camctrlnames[] = {
		"Scanning Mode", "Auto-Exposure Mode", "Auto-Exposure Priority",
		"Exposure Time (Absolute)", "Exposure Time (Relative)", "Focus (Absolute)",
		"Focus (Relative)", "Iris (Absolute)", "Iris (Relative)", "Zoom (Absolute)",
		"Zoom (Relative)", "PanTilt (Absolute)", "PanTilt (Relative)",
		"Roll (Absolute)", "Roll (Relative)", "Reserved", "Reserved", "Focus, Auto",
		"Privacy"
	};
	static const char * const stdnames[] = {
		"None", "NTSC - 525/60", "PAL - 625/50", "SECAM - 625/50",
		"NTSC - 625/50", "PAL - 525/60" };
	unsigned int i, ctrls, stds, n, p, termt, freq;
	char *term = NULL, termts[128];

	if (buf[1] != USB_DT_CS_INTERFACE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 3)
		printf("      Warning: Descriptor too short\n");
	printf("      VideoControl Interface Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bDescriptorSubtype  %5u ",
	       buf[0], buf[1], buf[2]);
	switch (buf[2]) {
	case 0x01:  /* HEADER */
		printf("(HEADER)\n");
		n = buf[11];
		if (buf[0] < 12+n)
			printf("      Warning: Descriptor too short\n");
		freq = buf[7] | (buf[8] << 8) | (buf[9] << 16) | (buf[10] << 24);
		printf("        bcdUVC              %2x.%02x\n"
		       "        wTotalLength        %5u\n"
		       "        dwClockFrequency    %5u.%06uMHz\n"
		       "        bInCollection       %5u\n",
		       buf[4], buf[3], buf[5] | (buf[6] << 8), freq / 1000000,
		       freq % 1000000, n);
		for (i = 0; i < n; i++)
			printf("        baInterfaceNr(%2u)   %5u\n", i, buf[12+i]);
		dump_junk(buf, "        ", 12+n);
		break;

	case 0x02:  /* INPUT_TERMINAL */
		printf("(INPUT_TERMINAL)\n");
		term = get_dev_string(dev, buf[7]);
		termt = buf[4] | (buf[5] << 8);
		n = termt == 0x0201 ? 7 : 0;
		get_videoterminal_string(termts, sizeof(termts), termt);
		if (buf[0] < 8 + n)
			printf("      Warning: Descriptor too short\n");
		printf("        bTerminalID         %5u\n"
		       "        wTerminalType      0x%04x %s\n"
		       "        bAssocTerminal      %5u\n",
		       buf[3], termt, termts, buf[6]);
		printf("        iTerminal           %5u %s\n",
		       buf[7], term);
		if (termt == 0x0201) {
			n += buf[14];
			printf("        wObjectiveFocalLengthMin  %5u\n"
			       "        wObjectiveFocalLengthMax  %5u\n"
			       "        wOcularFocalLength        %5u\n"
			       "        bControlSize              %5u\n",
			       buf[8] | (buf[9] << 8), buf[10] | (buf[11] << 8),
			       buf[12] | (buf[13] << 8), buf[14]);
			ctrls = 0;
			for (i = 0; i < 3 && i < buf[14]; i++)
				ctrls = (ctrls << 8) | buf[8+n-i-1];
			printf("        bmControls           0x%08x\n", ctrls);
			for (i = 0; i < 19; i++)
				if ((ctrls >> i) & 1)
					printf("          %s\n", camctrlnames[i]);
		}
		dump_junk(buf, "        ", 8+n);
		break;

	case 0x03:  /* OUTPUT_TERMINAL */
		printf("(OUTPUT_TERMINAL)\n");
		term = get_dev_string(dev, buf[8]);
		termt = buf[4] | (buf[5] << 8);
		get_audioterminal_string(termts, sizeof(termts), termt);
		if (buf[0] < 9)
			printf("      Warning: Descriptor too short\n");
		printf("        bTerminalID         %5u\n"
		       "        wTerminalType      0x%04x %s\n"
		       "        bAssocTerminal      %5u\n"
		       "        bSourceID           %5u\n"
		       "        iTerminal           %5u %s\n",
		       buf[3], termt, termts, buf[6], buf[7], buf[8], term);
		dump_junk(buf, "        ", 9);
		break;

	case 0x04:  /* SELECTOR_UNIT */
		printf("(SELECTOR_UNIT)\n");
		p = buf[4];
		if (buf[0] < 6+p)
			printf("      Warning: Descriptor too short\n");
		term = get_dev_string(dev, buf[5+p]);

		printf("        bUnitID             %5u\n"
		       "        bNrInPins           %5u\n",
		       buf[3], p);
		for (i = 0; i < p; i++)
			printf("        baSource(%2u)        %5u\n", i, buf[5+i]);
		printf("        iSelector           %5u %s\n",
		       buf[5+p], term);
		dump_junk(buf, "        ", 6+p);
		break;

	case 0x05:  /* PROCESSING_UNIT */
		printf("(PROCESSING_UNIT)\n");
		n = buf[7];
		term = get_dev_string(dev, buf[8+n]);
		if (buf[0] < 10+n)
			printf("      Warning: Descriptor too short\n");
		printf("        bUnitID             %5u\n"
		       "        bSourceID           %5u\n"
		       "        wMaxMultiplier      %5u\n"
		       "        bControlSize        %5u\n",
		       buf[3], buf[4], buf[5] | (buf[6] << 8), n);
		ctrls = 0;
		for (i = 0; i < 3 && i < n; i++)
			ctrls = (ctrls << 8) | buf[8+n-i-1];
		printf("        bmControls     0x%08x\n", ctrls);
		for (i = 0; i < 18; i++)
			if ((ctrls >> i) & 1)
				printf("          %s\n", ctrlnames[i]);
		stds = buf[9+n];
		printf("        iProcessing         %5u %s\n"
		       "        bmVideoStandards     0x%2x\n", buf[8+n], term, stds);
		for (i = 0; i < 6; i++)
			if ((stds >> i) & 1)
				printf("          %s\n", stdnames[i]);
		break;

	case 0x06:  /* EXTENSION_UNIT */
		printf("(EXTENSION_UNIT)\n");
		p = buf[21];
		n = buf[22+p];
		term = get_dev_string(dev, buf[23+p+n]);
		if (buf[0] < 24+p+n)
			printf("      Warning: Descriptor too short\n");
		printf("        bUnitID             %5u\n"
		       "        guidExtensionCode         %s\n"
		       "        bNumControl         %5u\n"
		       "        bNrPins             %5u\n",
		       buf[3], get_guid(&buf[4]), buf[20], buf[21]);
		for (i = 0; i < p; i++)
			printf("        baSourceID(%2u)      %5u\n", i, buf[22+i]);
		printf("        bControlSize        %5u\n", buf[22+p]);
		for (i = 0; i < n; i++)
			printf("        bmControls(%2u)       0x%02x\n", i, buf[23+p+i]);
		printf("        iExtension          %5u %s\n",
		       buf[23+p+n], term);
		dump_junk(buf, "        ", 24+p+n);
		break;

	default:
		printf("(unknown)\n"
		       "        Invalid desc subtype:");
		dump_bytes(buf+3, buf[0]-3);
		break;
	}

	free(term);
}

static void dump_videostreaming_interface(const unsigned char *buf)
{
	static const char * const colorPrims[] = { "Unspecified", "BT.709,sRGB",
		"BT.470-2 (M)", "BT.470-2 (B,G)", "SMPTE 170M", "SMPTE 240M" };
	static const char * const transferChars[] = { "Unspecified", "BT.709",
		"BT.470-2 (M)", "BT.470-2 (B,G)", "SMPTE 170M", "SMPTE 240M",
		"Linear", "sRGB"};
	static const char * const matrixCoeffs[] = { "Unspecified", "BT.709",
		"FCC", "BT.470-2 (B,G)", "SMPTE 170M (BT.601)", "SMPTE 240M" };
	unsigned int i, m, n, p, flags, len;

	if (buf[1] != USB_DT_CS_INTERFACE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 3)
		printf("      Warning: Descriptor too short\n");
	printf("      VideoStreaming Interface Descriptor:\n"
	       "        bLength                         %5u\n"
	       "        bDescriptorType                 %5u\n"
	       "        bDescriptorSubtype              %5u ",
	       buf[0], buf[1], buf[2]);
	switch (buf[2]) {
	case 0x01: /* INPUT_HEADER */
		printf("(INPUT_HEADER)\n");
		p = buf[3];
		n = buf[12];
		if (buf[0] < 13+p*n)
			printf("      Warning: Descriptor too short\n");
		printf("        bNumFormats                     %5u\n"
		       "        wTotalLength                    %5u\n"
		       "        bEndPointAddress                %5u\n"
		       "        bmInfo                          %5u\n"
		       "        bTerminalLink                   %5u\n"
		       "        bStillCaptureMethod             %5u\n"
		       "        bTriggerSupport                 %5u\n"
		       "        bTriggerUsage                   %5u\n"
		       "        bControlSize                    %5u\n",
		       p, buf[4] | (buf[5] << 8), buf[6], buf[7], buf[8],
		       buf[9], buf[10], buf[11], n);
		for (i = 0; i < p; i++)
			printf(
			"        bmaControls(%2u)                 %5u\n",
				i, buf[13+p*n]);
		dump_junk(buf, "        ", 13+p*n);
		break;

	case 0x02: /* OUTPUT_HEADER */
		printf("(OUTPUT_HEADER)\n");
		p = buf[3];
		n = buf[8];
		if (buf[0] < 9+p*n)
			printf("      Warning: Descriptor too short\n");
		printf("        bNumFormats                 %5u\n"
		       "        wTotalLength                %5u\n"
		       "        bEndpointAddress            %5u\n"
		       "        bTerminalLink               %5u\n"
		       "        bControlSize                %5u\n",
		       p, buf[4] | (buf[5] << 8), buf[6], buf[7], n);
		for (i = 0; i < p; i++)
			printf(
			"        bmaControls(%2u)             %5u\n",
				i, buf[9+p*n]);
		dump_junk(buf, "        ", 9+p*n);
		break;

	case 0x03: /* STILL_IMAGE_FRAME */
		printf("(STILL_IMAGE_FRAME)\n");
		n = buf[4];
		m = buf[5+4*n];
		if (buf[0] < 6+4*n+m)
			printf("      Warning: Descriptor too short\n");
		printf("        bEndpointAddress                %5u\n"
		       "        bNumImageSizePatterns             %3u\n",
		       buf[3], n);
		for (i = 0; i < n; i++)
			printf("        wWidth(%2u)                      %5u\n"
			       "        wHeight(%2u)                     %5u\n",
			       i, buf[5+4*i] | (buf[6+4*i] << 8),
			       i, buf[7+4*i] | (buf[8+4*i] << 8));
		printf("        bNumCompressionPatterns           %3u\n", n);
		for (i = 0; i < m; i++)
			printf("        bCompression(%2u)                %5u\n",
			       i, buf[6+4*n+i]);
		dump_junk(buf, "        ", 6+4*n+m);
		break;

	case 0x04: /* FORMAT_UNCOMPRESSED */
	case 0x10: /* FORMAT_FRAME_BASED */
		if (buf[2] == 0x04) {
			printf("(FORMAT_UNCOMPRESSED)\n");
			len = 27;
		} else {
			printf("(FORMAT_FRAME_BASED)\n");
			len = 28;
		}
		if (buf[0] < len)
			printf("      Warning: Descriptor too short\n");
		flags = buf[25];
		printf("        bFormatIndex                    %5u\n"
		       "        bNumFrameDescriptors            %5u\n"
		       "        guidFormat                            %s\n"
		       "        bBitsPerPixel                   %5u\n"
		       "        bDefaultFrameIndex              %5u\n"
		       "        bAspectRatioX                   %5u\n"
		       "        bAspectRatioY                   %5u\n"
		       "        bmInterlaceFlags                 0x%02x\n",
		       buf[3], buf[4], get_guid(&buf[5]), buf[21], buf[22],
		       buf[23], buf[24], flags);
		printf("          Interlaced stream or variable: %s\n",
		       (flags & (1 << 0)) ? "Yes" : "No");
		printf("          Fields per frame: %u fields\n",
		       (flags & (1 << 1)) ? 1 : 2);
		printf("          Field 1 first: %s\n",
		       (flags & (1 << 2)) ? "Yes" : "No");
		printf("          Field pattern: ");
		switch ((flags >> 4) & 0x03) {
		case 0:
			printf("Field 1 only\n");
			break;
		case 1:
			printf("Field 2 only\n");
			break;
		case 2:
			printf("Regular pattern of fields 1 and 2\n");
			break;
		case 3:
			printf("Random pattern of fields 1 and 2\n");
			break;
		}
		printf("          bCopyProtect                  %5u\n", buf[26]);
		if (buf[2] == 0x10)
			printf("          bVariableSize                 %5u\n", buf[27]);
		dump_junk(buf, "        ", len);
		break;

	case 0x05: /* FRAME UNCOMPRESSED */
	case 0x07: /* FRAME_MJPEG */
	case 0x11: /* FRAME_FRAME_BASED */
		if (buf[2] == 0x05) {
			printf("(FRAME_UNCOMPRESSED)\n");
			n = 25;
		} else if (buf[2] == 0x07) {
			printf("(FRAME_MJPEG)\n");
			n = 25;
		} else {
			printf("(FRAME_FRAME_BASED)\n");
			n = 21;
		}
		len = (buf[n] != 0) ? (26+buf[n]*4) : 38;
		if (buf[0] < len)
			printf("      Warning: Descriptor too short\n");
		flags = buf[4];
		printf("        bFrameIndex                     %5u\n"
		       "        bmCapabilities                   0x%02x\n",
		       buf[3], flags);
		printf("          Still image %ssupported\n",
		       (flags & (1 << 0)) ? "" : "un");
		if (flags & (1 << 1))
			printf("          Fixed frame-rate\n");
		printf("        wWidth                          %5u\n"
		       "        wHeight                         %5u\n"
		       "        dwMinBitRate                %9u\n"
		       "        dwMaxBitRate                %9u\n",
		       buf[5] | (buf[6] <<  8), buf[7] | (buf[8] << 8),
		       buf[9] | (buf[10] << 8) | (buf[11] << 16) | (buf[12] << 24),
		       buf[13] | (buf[14] << 8) | (buf[15] << 16) | (buf[16] << 24));
		if (buf[2] == 0x11)
			printf("        dwDefaultFrameInterval      %9u\n"
			       "        bFrameIntervalType              %5u\n"
			       "        dwBytesPerLine              %9u\n",
			       buf[17] | (buf[18] << 8) | (buf[19] << 16) | (buf[20] << 24),
			       buf[21],
			       buf[22] | (buf[23] << 8) | (buf[24] << 16) | (buf[25] << 24));
		else
			printf("        dwMaxVideoFrameBufferSize   %9u\n"
			       "        dwDefaultFrameInterval      %9u\n"
			       "        bFrameIntervalType              %5u\n",
			       buf[17] | (buf[18] << 8) | (buf[19] << 16) | (buf[20] << 24),
			       buf[21] | (buf[22] << 8) | (buf[23] << 16) | (buf[24] << 24),
			       buf[25]);
		if (buf[n] == 0)
			printf("        dwMinFrameInterval          %9u\n"
			       "        dwMaxFrameInterval          %9u\n"
			       "        dwFrameIntervalStep         %9u\n",
			       buf[26] | (buf[27] << 8) | (buf[28] << 16) | (buf[29] << 24),
			       buf[30] | (buf[31] << 8) | (buf[32] << 16) | (buf[33] << 24),
			       buf[34] | (buf[35] << 8) | (buf[36] << 16) | (buf[37] << 24));
		else
			for (i = 0; i < buf[n]; i++)
				printf("        dwFrameInterval(%2u)         %9u\n",
				       i, buf[26+4*i] | (buf[27+4*i] << 8) |
				       (buf[28+4*i] << 16) | (buf[29+4*i] << 24));
		dump_junk(buf, "        ", len);
		break;

	case 0x06: /* FORMAT_MJPEG */
		printf("(FORMAT_MJPEG)\n");
		if (buf[0] < 11)
			printf("      Warning: Descriptor too short\n");
		flags = buf[5];
		printf("        bFormatIndex                    %5u\n"
		       "        bNumFrameDescriptors            %5u\n"
		       "        bFlags                          %5u\n",
		       buf[3], buf[4], flags);
		printf("          Fixed-size samples: %s\n",
		       (flags & (1 << 0)) ? "Yes" : "No");
		flags = buf[9];
		printf("        bDefaultFrameIndex              %5u\n"
		       "        bAspectRatioX                   %5u\n"
		       "        bAspectRatioY                   %5u\n"
		       "        bmInterlaceFlags                 0x%02x\n",
		       buf[6], buf[7], buf[8], flags);
		printf("          Interlaced stream or variable: %s\n",
		       (flags & (1 << 0)) ? "Yes" : "No");
		printf("          Fields per frame: %u fields\n",
		       (flags & (1 << 1)) ? 2 : 1);
		printf("          Field 1 first: %s\n",
		       (flags & (1 << 2)) ? "Yes" : "No");
		printf("          Field pattern: ");
		switch ((flags >> 4) & 0x03) {
		case 0:
			printf("Field 1 only\n");
			break;
		case 1:
			printf("Field 2 only\n");
			break;
		case 2:
			printf("Regular pattern of fields 1 and 2\n");
			break;
		case 3:
			printf("Random pattern of fields 1 and 2\n");
			break;
		}
		printf("          bCopyProtect                  %5u\n", buf[10]);
		dump_junk(buf, "        ", 11);
		break;

	case 0x0a: /* FORMAT_MPEG2TS */
		printf("(FORMAT_MPEG2TS)\n");
		len = buf[0] < 23 ? 7 : 23;
		if (buf[0] < len)
			printf("      Warning: Descriptor too short\n");
		printf("        bFormatIndex                    %5u\n"
		       "        bDataOffset                     %5u\n"
		       "        bPacketLength                   %5u\n"
		       "        bStrideLength                   %5u\n",
		       buf[3], buf[4], buf[5], buf[6]);
		if (len > 7)
			printf("        guidStrideFormat                      %s\n",
			       get_guid(&buf[7]));
		dump_junk(buf, "        ", len);
		break;

	case 0x0d: /* COLORFORMAT */
		printf("(COLORFORMAT)\n");
		if (buf[0] < 6)
			printf("      Warning: Descriptor too short\n");
		printf("        bColorPrimaries                 %5u (%s)\n",
		       buf[3], (buf[3] <= 5) ? colorPrims[buf[3]] : "Unknown");
		printf("        bTransferCharacteristics        %5u (%s)\n",
		       buf[4], (buf[4] <= 7) ? transferChars[buf[4]] : "Unknown");
		printf("        bMatrixCoefficients             %5u (%s)\n",
		       buf[5], (buf[5] <= 5) ? matrixCoeffs[buf[5]] : "Unknown");
		dump_junk(buf, "        ", 6);
		break;

	default:
		printf("        Invalid desc subtype:");
		dump_bytes(buf+3, buf[0]-3);
		break;
	}
}

static void dump_dfu_interface(const unsigned char *buf)
{
	if (buf[1] != USB_DT_CS_DEVICE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 7)
		printf("      Warning: Descriptor too short\n");
	printf("      Device Firmware Upgrade Interface Descriptor:\n"
	       "        bLength                         %5u\n"
	       "        bDescriptorType                 %5u\n"
	       "        bmAttributes                    %5u\n",
	       buf[0], buf[1], buf[2]);
	if (buf[2] & 0xf0)
		printf("          (unknown attributes!)\n");
	printf("          Will %sDetach\n", (buf[2] & 0x08) ? "" : "Not ");
	printf("          Manifestation %s\n", (buf[2] & 0x04) ? "Tolerant" : "Intolerant");
	printf("          Upload %s\n", (buf[2] & 0x02) ? "Supported" : "Unsupported");
	printf("          Download %s\n", (buf[2] & 0x01) ? "Supported" : "Unsupported");
	printf("        wDetachTimeout                  %5u milliseconds\n"
	       "        wTransferSize                   %5u bytes\n",
	       buf[3] | (buf[4] << 8), buf[5] | (buf[6] << 8));

	/* DFU 1.0 defines no version code, DFU 1.1 does */
	if (buf[0] < 9)
		return;
	printf("        bcdDFUVersion                   %x.%02x\n",
			buf[8], buf[7]);
}

static void dump_hub(const char *prefix, const unsigned char *p, int tt_type)
{
	unsigned int l, i, j;
	unsigned int offset;
	unsigned int wHubChar = (p[4] << 8) | p[3];

	printf("%sHub Descriptor:\n", prefix);
	printf("%s  bLength             %3u\n", prefix, p[0]);
	printf("%s  bDescriptorType     %3u\n", prefix, p[1]);
	printf("%s  nNbrPorts           %3u\n", prefix, p[2]);
	printf("%s  wHubCharacteristic 0x%04x\n", prefix, wHubChar);
	switch (wHubChar & 0x03) {
	case 0:
		printf("%s    Ganged power switching\n", prefix);
		break;
	case 1:
		printf("%s    Per-port power switching\n", prefix);
		break;
	default:
		printf("%s    No power switching (usb 1.0)\n", prefix);
		break;
	}
	if (wHubChar & 0x04)
		printf("%s    Compound device\n", prefix);
	switch ((wHubChar >> 3) & 0x03) {
	case 0:
		printf("%s    Ganged overcurrent protection\n", prefix);
		break;
	case 1:
		printf("%s    Per-port overcurrent protection\n", prefix);
		break;
	default:
		printf("%s    No overcurrent protection\n", prefix);
		break;
	}
	/* USB 3.0 hubs don't have TTs. */
	if (tt_type >= 1 && tt_type < 3) {
		l = (wHubChar >> 5) & 0x03;
		printf("%s    TT think time %d FS bits\n", prefix, (l + 1) * 8);
	}
	/* USB 3.0 hubs don't have port indicators.  Sad face. */
	if (tt_type != 3 && wHubChar & (1<<7))
		printf("%s    Port indicators\n", prefix);
	printf("%s  bPwrOn2PwrGood      %3u * 2 milli seconds\n", prefix, p[5]);

	/* USB 3.0 hubs report current in units of aCurrentUnit, or 4 mA */
	if (tt_type == 3)
		printf("%s  bHubContrCurrent   %4u milli Ampere\n",
				prefix, p[6]*4);
	else
		printf("%s  bHubContrCurrent    %3u milli Ampere\n",
				prefix, p[6]);

	if (tt_type == 3) {
		printf("%s  bHubDecLat          0.%1u micro seconds\n",
				prefix, p[7]);
		printf("%s  wHubDelay          %4u nano seconds\n",
				prefix, (p[8] << 4) +(p[7]));
		offset = 10;
	} else {
		offset = 7;
	}

	l = (p[2] >> 3) + 1; /* this determines the variable number of bytes following */
	if (l > HUB_STATUS_BYTELEN)
		l = HUB_STATUS_BYTELEN;
	printf("%s  DeviceRemovable   ", prefix);
	for (i = 0; i < l; i++)
		printf(" 0x%02x", p[offset+i]);

	if (tt_type != 3) {
		printf("\n%s  PortPwrCtrlMask   ", prefix);
		for (j = 0; j < l; j++)
			printf(" 0x%02x", p[offset+i+j]);
	}
	printf("\n");
}

static void dump_ccid_device(const unsigned char *buf)
{
	unsigned int us;

	if (buf[0] < 54) {
		printf("      Warning: Descriptor too short\n");
		return;
	}
	printf("      ChipCard Interface Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bcdCCID             %2x.%02x",
	       buf[0], buf[1], buf[3], buf[2]);
	if (buf[3] != 1 || buf[2] != 0)
		fputs("  (Warning: Only accurate for version 1.0)", stdout);
	putchar('\n');

	printf("        nMaxSlotIndex       %5u\n"
		"        bVoltageSupport     %5u  %s%s%s\n",
		buf[4],
		buf[5],
	       (buf[5] & 1) ? "5.0V " : "",
	       (buf[5] & 2) ? "3.0V " : "",
	       (buf[5] & 4) ? "1.8V " : "");

	us = convert_le_u32 (buf+6);
	printf("        dwProtocols         %5u ", us);
	if ((us & 1))
		fputs(" T=0", stdout);
	if ((us & 2))
		fputs(" T=1", stdout);
	if ((us & ~3))
		fputs(" (Invalid values detected)", stdout);
	putchar('\n');

	us = convert_le_u32(buf+10);
	printf("        dwDefaultClock      %5u\n", us);
	us = convert_le_u32(buf+14);
	printf("        dwMaxiumumClock     %5u\n", us);
	printf("        bNumClockSupported  %5u\n", buf[18]);
	us = convert_le_u32(buf+19);
	printf("        dwDataRate        %7u bps\n", us);
	us = convert_le_u32(buf+23);
	printf("        dwMaxDataRate     %7u bps\n", us);
	printf("        bNumDataRatesSupp.  %5u\n", buf[27]);

	us = convert_le_u32(buf+28);
	printf("        dwMaxIFSD           %5u\n", us);

	us = convert_le_u32(buf+32);
	printf("        dwSyncProtocols  %08X ", us);
	if ((us&1))
		fputs(" 2-wire", stdout);
	if ((us&2))
		fputs(" 3-wire", stdout);
	if ((us&4))
		fputs(" I2C", stdout);
	putchar('\n');

	us = convert_le_u32(buf+36);
	printf("        dwMechanical     %08X ", us);
	if ((us & 1))
		fputs(" accept", stdout);
	if ((us & 2))
		fputs(" eject", stdout);
	if ((us & 4))
		fputs(" capture", stdout);
	if ((us & 8))
		fputs(" lock", stdout);
	putchar('\n');

	us = convert_le_u32(buf+40);
	printf("        dwFeatures       %08X\n", us);
	if ((us & 0x0002))
		fputs("          Auto configuration based on ATR\n", stdout);
	if ((us & 0x0004))
		fputs("          Auto activation on insert\n", stdout);
	if ((us & 0x0008))
		fputs("          Auto voltage selection\n", stdout);
	if ((us & 0x0010))
		fputs("          Auto clock change\n", stdout);
	if ((us & 0x0020))
		fputs("          Auto baud rate change\n", stdout);
	if ((us & 0x0040))
		fputs("          Auto parameter negotation made by CCID\n", stdout);
	else if ((us & 0x0080))
		fputs("          Auto PPS made by CCID\n", stdout);
	else if ((us & (0x0040 | 0x0080)))
		fputs("        WARNING: conflicting negotation features\n", stdout);

	if ((us & 0x0100))
		fputs("          CCID can set ICC in clock stop mode\n", stdout);
	if ((us & 0x0200))
		fputs("          NAD value other than 0x00 accepted\n", stdout);
	if ((us & 0x0400))
		fputs("          Auto IFSD exchange\n", stdout);

	if ((us & 0x00010000))
		fputs("          TPDU level exchange\n", stdout);
	else if ((us & 0x00020000))
		fputs("          Short APDU level exchange\n", stdout);
	else if ((us & 0x00040000))
		fputs("          Short and extended APDU level exchange\n", stdout);
	else if ((us & 0x00070000))
		fputs("        WARNING: conflicting exchange levels\n", stdout);

	us = convert_le_u32(buf+44);
	printf("        dwMaxCCIDMsgLen     %5u\n", us);

	printf("        bClassGetResponse    ");
	if (buf[48] == 0xff)
		fputs("echo\n", stdout);
	else
		printf("  %02X\n", buf[48]);

	printf("        bClassEnvelope       ");
	if (buf[49] == 0xff)
		fputs("echo\n", stdout);
	else
		printf("  %02X\n", buf[48]);

	printf("        wlcdLayout           ");
	if (!buf[50] && !buf[51])
		fputs("none\n", stdout);
	else
		printf("%u cols %u lines\n", buf[50], buf[51]);

	printf("        bPINSupport         %5u ", buf[52]);
	if ((buf[52] & 1))
		fputs(" verification", stdout);
	if ((buf[52] & 2))
		fputs(" modification", stdout);
	putchar('\n');

	printf("        bMaxCCIDBusySlots   %5u\n", buf[53]);

	if (buf[0] > 54) {
		fputs("        junk             ", stdout);
		dump_bytes(buf+54, buf[0]-54);
	}
}

/* ---------------------------------------------------------------------- */

/*
 * HID descriptor
 */

static void dump_report_desc(unsigned char *b, int l)
{
	unsigned int j, bsize, btag, btype, data = 0xffff, hut = 0xffff;
	int i;
	char *types[4] = { "Main", "Global", "Local", "reserved" };
	char indent[] = "                            ";

	printf("          Report Descriptor: (length is %d)\n", l);
	for (i = 0; i < l; ) {
		bsize = b[i] & 0x03;
		if (bsize == 3)
			bsize = 4;
		btype = b[i] & (0x03 << 2);
		btag = b[i] & ~0x03; /* 2 LSB bits encode length */
		printf("            Item(%-6s): %s, data=", types[btype>>2],
				names_reporttag(btag));
		if (bsize > 0) {
			printf(" [ ");
			data = 0;
			for (j = 0; j < bsize; j++) {
				printf("0x%02x ", b[i+1+j]);
				data += (b[i+1+j] << (8*j));
			}
			printf("] %d", data);
		} else
			printf("none");
		printf("\n");
		switch (btag) {
		case 0x04: /* Usage Page */
			printf("%s%s\n", indent, names_huts(data));
			hut = data;
			break;

		case 0x08: /* Usage */
		case 0x18: /* Usage Minimum */
		case 0x28: /* Usage Maximum */
			printf("%s%s\n", indent,
			       names_hutus((hut << 16) + data));
			break;

		case 0x54: /* Unit Exponent */
			printf("%sUnit Exponent: %i\n", indent,
			       (signed char)data);
			break;

		case 0x64: /* Unit */
			printf("%s", indent);
			dump_unit(data, bsize);
			break;

		case 0xa0: /* Collection */
			printf("%s", indent);
			switch (data) {
			case 0x00:
				printf("Physical\n");
				break;

			case 0x01:
				printf("Application\n");
				break;

			case 0x02:
				printf("Logical\n");
				break;

			case 0x03:
				printf("Report\n");
				break;

			case 0x04:
				printf("Named Array\n");
				break;

			case 0x05:
				printf("Usage Switch\n");
				break;

			case 0x06:
				printf("Usage Modifier\n");
				break;

			default:
				if (data & 0x80)
					printf("Vendor defined\n");
				else
					printf("Reserved for future use.\n");
			}
			break;
		case 0x80: /* Input */
		case 0x90: /* Output */
		case 0xb0: /* Feature */
			printf("%s%s %s %s %s %s\n%s%s %s %s %s\n",
			       indent,
			       data & 0x01 ? "Constant" : "Data",
			       data & 0x02 ? "Variable" : "Array",
			       data & 0x04 ? "Relative" : "Absolute",
			       data & 0x08 ? "Wrap" : "No_Wrap",
			       data & 0x10 ? "Non_Linear" : "Linear",
			       indent,
			       data & 0x20 ? "No_Preferred_State" : "Preferred_State",
			       data & 0x40 ? "Null_State" : "No_Null_Position",
			       data & 0x80 ? "Volatile" : "Non_Volatile",
			       data & 0x100 ? "Buffered Bytes" : "Bitfield");
			break;
		}
		i += 1 + bsize;
	}
}

static void dump_hid_device(libusb_device_handle *dev,
			    const struct libusb_interface_descriptor *interface,
			    const unsigned char *buf)
{
	unsigned int i, len;
	unsigned int n;
	unsigned char dbuf[8192];

	if (buf[1] != LIBUSB_DT_HID)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 6+3*buf[5])
		printf("      Warning: Descriptor too short\n");
	printf("        HID Device Descriptor:\n"
	       "          bLength             %5u\n"
	       "          bDescriptorType     %5u\n"
	       "          bcdHID              %2x.%02x\n"
	       "          bCountryCode        %5u %s\n"
	       "          bNumDescriptors     %5u\n",
	       buf[0], buf[1], buf[3], buf[2], buf[4],
	       names_countrycode(buf[4]) ? : "Unknown", buf[5]);
	for (i = 0; i < buf[5]; i++)
		printf("          bDescriptorType     %5u %s\n"
		       "          wDescriptorLength   %5u\n",
		       buf[6+3*i], names_hid(buf[6+3*i]),
		       buf[7+3*i] | (buf[8+3*i] << 8));
	dump_junk(buf, "        ", 6+3*buf[5]);
	if (!do_report_desc)
		return;

	if (!dev) {
		printf("         Report Descriptors: \n"
		       "           ** UNAVAILABLE **\n");
		return;
	}

	for (i = 0; i < buf[5]; i++) {
		/* we are just interested in report descriptors*/
		if (buf[6+3*i] != LIBUSB_DT_REPORT)
			continue;
		len = buf[7+3*i] | (buf[8+3*i] << 8);
		if (len > (unsigned int)sizeof(dbuf)) {
			printf("report descriptor too long\n");
			continue;
		}
		if (libusb_claim_interface(dev, interface->bInterfaceNumber) == 0) {
			int retries = 4;
			n = 0;
			while (n < len && retries--)
				n = usb_control_msg(dev,
					 LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD
						| LIBUSB_RECIPIENT_INTERFACE,
					 LIBUSB_REQUEST_GET_DESCRIPTOR,
					 (LIBUSB_DT_REPORT << 8),
					 interface->bInterfaceNumber,
					 dbuf, len,
					 CTRL_TIMEOUT);

			if (n > 0) {
				if (n < len)
					printf("          Warning: incomplete report descriptor\n");
				dump_report_desc(dbuf, n);
			}
			libusb_release_interface(dev, interface->bInterfaceNumber);
		} else {
			/* recent Linuxes require claim() for RECIP_INTERFACE,
			 * so "rmmod hid" will often make these available.
			 */
			printf("         Report Descriptors: \n"
			       "           ** UNAVAILABLE **\n");
		}
	}
}

static char *
dump_comm_descriptor(libusb_device_handle *dev, const unsigned char *buf, char *indent)
{
	int		tmp;
	char		*str = NULL;
	char		*type;

	switch (buf[2]) {
	case 0:
		type = "Header";
		if (buf[0] != 5)
			goto bad;
		printf("%sCDC Header:\n"
		       "%s  bcdCDC               %x.%02x\n",
		       indent,
		       indent, buf[4], buf[3]);
		break;
	case 0x01:		/* call management functional desc */
		type = "Call Management";
		if (buf[0] != 5)
			goto bad;
		printf("%sCDC Call Management:\n"
		       "%s  bmCapabilities       0x%02x\n",
		       indent,
		       indent, buf[3]);
		if (buf[3] & 0x01)
			printf("%s    call management\n", indent);
		if (buf[3] & 0x02)
			printf("%s    use DataInterface\n", indent);
		printf("%s  bDataInterface          %d\n", indent, buf[4]);
		break;
	case 0x02:		/* acm functional desc */
		type = "ACM";
		if (buf[0] != 4)
			goto bad;
		printf("%sCDC ACM:\n"
		       "%s  bmCapabilities       0x%02x\n",
		       indent,
		       indent, buf[3]);
		if (buf[3] & 0x08)
			printf("%s    connection notifications\n", indent);
		if (buf[3] & 0x04)
			printf("%s    sends break\n", indent);
		if (buf[3] & 0x02)
			printf("%s    line coding and serial state\n", indent);
		if (buf[3] & 0x01)
			printf("%s    get/set/clear comm features\n", indent);
		break;
#if 0
	case 0x03:		/* direct line management */
	case 0x04:		/* telephone ringer */
	case 0x05:		/* telephone call and line state reporting */
#endif
	case 0x06:		/* union desc */
		type = "Union";
		if (buf[0] < 5)
			goto bad;
		printf("%sCDC Union:\n"
		       "%s  bMasterInterface        %d\n"
		       "%s  bSlaveInterface         ",
		       indent,
		       indent, buf[3],
		       indent);
		for (tmp = 4; tmp < buf[0]; tmp++)
			printf("%d ", buf[tmp]);
		printf("\n");
		break;
	case 0x07:		/* country selection functional desc */
		type = "Country Selection";
		if (buf[0] < 6 || (buf[0] & 1) != 0)
			goto bad;
		str = get_dev_string(dev, buf[3]);
		printf("%sCountry Selection:\n"
		       "%s  iCountryCodeRelDate     %4d %s\n",
		       indent,
		       indent, buf[3], (buf[3] && *str) ? str : "(?\?)");
		for (tmp = 4; tmp < buf[0]; tmp += 2) {
			printf("%s  wCountryCode          0x%02x%02x\n",
				indent, buf[tmp], buf[tmp + 1]);
		}
		break;
	case 0x08:		/* telephone operational modes */
		type = "Telephone Operations";
		if (buf[0] != 4)
			goto bad;
		printf("%sCDC Telephone operations:\n"
		       "%s  bmCapabilities       0x%02x\n",
		       indent,
		       indent, buf[3]);
		if (buf[3] & 0x04)
			printf("%s    computer centric mode\n", indent);
		if (buf[3] & 0x02)
			printf("%s    standalone mode\n", indent);
		if (buf[3] & 0x01)
			printf("%s    simple mode\n", indent);
		break;
#if 0
	case 0x09:		/* USB terminal */
#endif
	case 0x0a:		/* network channel terminal */
		type = "Network Channel Terminal";
		if (buf[0] != 7)
			goto bad;
		str = get_dev_string(dev, buf[4]);
		printf("%sNetwork Channel Terminal:\n"
		       "%s  bEntityId               %3d\n"
		       "%s  iName                   %3d %s\n"
		       "%s  bChannelIndex           %3d\n"
		       "%s  bPhysicalInterface      %3d\n",
		       indent,
		       indent, buf[3],
		       indent, buf[4], str,
		       indent, buf[5],
		       indent, buf[6]);
		break;
#if 0
	case 0x0b:		/* protocol unit */
	case 0x0c:		/* extension unit */
	case 0x0d:		/* multi-channel management */
	case 0x0e:		/* CAPI control management*/
#endif
	case 0x0f:		/* ethernet functional desc */
		type = "Ethernet";
		if (buf[0] != 13)
			goto bad;
		str = get_dev_string(dev, buf[3]);
		tmp = buf[7] << 8;
		tmp |= buf[6]; tmp <<= 8;
		tmp |= buf[5]; tmp <<= 8;
		tmp |= buf[4];
		printf("%sCDC Ethernet:\n"
		       "%s  iMacAddress             %10d %s\n"
		       "%s  bmEthernetStatistics    0x%08x\n",
		       indent,
		       indent, buf[3], (buf[3] && *str) ? str : "(?\?)",
		       indent, tmp);
		/* FIXME dissect ALL 28 bits */
		printf("%s  wMaxSegmentSize         %10d\n"
		       "%s  wNumberMCFilters            0x%04x\n"
		       "%s  bNumberPowerFilters     %10d\n",
		       indent, (buf[9]<<8)|buf[8],
		       indent, (buf[11]<<8)|buf[10],
		       indent, buf[12]);
		break;
#if 0
	case 0x10:		/* ATM networking */
#endif
	case 0x11:		/* WHCM functional desc */
		type = "WHCM version";
		if (buf[0] != 5)
			goto bad;
		printf("%sCDC WHCM:\n"
		       "%s  bcdVersion           %x.%02x\n",
		       indent,
		       indent, buf[4], buf[3]);
		break;
	case 0x12:		/* MDLM functional desc */
		type = "MDLM";
		if (buf[0] != 21)
			goto bad;
		printf("%sCDC MDLM:\n"
		       "%s  bcdCDC               %x.%02x\n"
		       "%s  bGUID               %s\n",
		       indent,
		       indent, buf[4], buf[3],
		       indent, get_guid(buf + 5));
		break;
	case 0x13:		/* MDLM detail desc */
		type = "MDLM detail";
		if (buf[0] < 5)
			goto bad;
		printf("%sCDC MDLM detail:\n"
		       "%s  bGuidDescriptorType  %02x\n"
		       "%s  bDetailData         ",
		       indent,
		       indent, buf[3],
		       indent);
		dump_bytes(buf + 4, buf[0] - 4);
		break;
	case 0x14:		/* device management functional desc */
		type = "Device Management";
		if (buf[0] != 7)
			goto bad;
		printf("%sCDC Device Management:\n"
		       "%s  bcdVersion           %x.%02x\n"
		       "%s  wMaxCommand          %d\n",
		       indent,
		       indent, buf[4], buf[3],
		       indent, (buf[6] << 8) | buf[5]);
		break;
	case 0x15:		/* OBEX functional desc */
		type = "OBEX";
		if (buf[0] != 5)
			goto bad;
		printf("%sCDC OBEX:\n"
		       "%s  bcdVersion           %x.%02x\n",
		       indent,
		       indent, buf[4], buf[3]);
		break;
	case 0x16:		/* command set functional desc */
		type = "Command Set";
		if (buf[0] != 22)
			goto bad;
		str = get_dev_string(dev, buf[5]);
		printf("%sCDC Command Set:\n"
		       "%s  bcdVersion           %x.%02x\n"
		       "%s  iCommandSet          %4d %s\n"
		       "%s  bGUID                %s\n",
		       indent,
		       indent, buf[4], buf[3],
		       indent, buf[5], (buf[5] && *str) ? str : "(?\?)",
		       indent, get_guid(buf + 6));
		break;
#if 0
	case 0x17:		/* command set detail desc */
	case 0x18:		/* telephone control model functional desc */
#endif
	case 0x1a:		/* NCM functional desc */
		type = "NCM";
		if (buf[0] != 6)
			goto bad;
		printf("%sCDC NCM:\n"
		       "%s  bcdNcmVersion        %x.%02x\n"
		       "%s  bmNetworkCapabilities 0x%02x\n",
		       indent,
		       indent, buf[4], buf[3],
		       indent, buf[5]);
		if (buf[5] & 1<<5)
			printf("%s    8-byte ntb input size\n", indent);
		if (buf[5] & 1<<4)
			printf("%s    crc mode\n", indent);
		if (buf[5] & 1<<3)
			printf("%s    max datagram size\n", indent);
		if (buf[5] & 1<<2)
			printf("%s    encapsulated commands\n", indent);
		if (buf[5] & 1<<1)
			printf("%s    net address\n", indent);
		if (buf[5] & 1<<0)
			printf("%s    packet filter\n", indent);
		break;
	case 0x1b:		/* MBIM functional desc */
		type = "MBIM";
		if (buf[0] != 12)
			goto bad;
		printf("%sCDC MBIM:\n"
		       "%s  bcdMBIMVersion       %x.%02x\n"
		       "%s  wMaxControlMessage   %d\n"
		       "%s  bNumberFilters       %d\n"
		       "%s  bMaxFilterSize       %d\n"
		       "%s  wMaxSegmentSize      %d\n"
		       "%s  bmNetworkCapabilities 0x%02x\n",
		       indent,
		       indent, buf[4], buf[3],
		       indent, (buf[6] << 8) | buf[5],
		       indent, buf[7],
		       indent, buf[8],
		       indent, (buf[10] << 8) | buf[9],
		       indent, buf[11]);
		if (buf[11] & 0x20)
			printf("%s    8-byte ntb input size\n", indent);
		if (buf[11] & 0x08)
			printf("%s    max datagram size\n", indent);
		break;
	case 0x1c:		/* MBIM extended functional desc */
		type = "MBIM Extended";
		if (buf[0] != 8)
			goto bad;
		printf("%sCDC MBIM Extended:\n"
		       "%s  bcdMBIMExtendedVersion          %2x.%02x\n"
		       "%s  bMaxOutstandingCommandMessages    %3d\n"
		       "%s  wMTU                            %5d\n",
		       indent,
		       indent, buf[4], buf[3],
		       indent, buf[5],
		       indent, buf[6] | (buf[7] << 8));
		break;
	default:
		/* FIXME there are about a dozen more descriptor types */
		printf("%sUNRECOGNIZED CDC: ", indent);
		dump_bytes(buf, buf[0]);
		return "unrecognized comm descriptor";
	}

	free(str);

	return 0;

bad:
	printf("%sINVALID CDC (%s): ", indent, type);
	dump_bytes(buf, buf[0]);
	return "corrupt comm descriptor";
}

/* ---------------------------------------------------------------------- */

static void do_hub(libusb_device_handle *fd, unsigned tt_type, unsigned speed)
{
	unsigned char buf[7 /* base descriptor */
			+ 2 /* bitmasks */ * HUB_STATUS_BYTELEN];
	int i, ret, value;
	unsigned int link_state;
	char *link_state_descriptions[] = {
		" U0",
		" U1",
		" U2",
		" suspend",
		" SS.disabled",
		" Rx.Detect",
		" SS.Inactive",
		" Polling",
		" Recovery",
		" Hot Reset",
		" Compliance",
		" Loopback",
	};

	/* USB 3.0 hubs have a slightly different descriptor */
	if (speed == 0x0300)
		value = 0x2A;
	else
		value = 0x29;

	ret = usb_control_msg(fd,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			value << 8, 0,
			buf, sizeof buf, CTRL_TIMEOUT);
	if (ret < 0) {
		/* Linux returns EHOSTUNREACH for suspended devices */
		if (errno != EHOSTUNREACH)
			fprintf(stderr, "can't get hub descriptor, %s (%s)\n",
				libusb_error_name(ret), strerror(errno));
		return;
	}
	if (ret < 9 /* at least one port's bitmasks */) {
		fprintf(stderr,
			"incomplete hub descriptor, %d bytes\n",
			ret);
		return;
	}
	dump_hub("", buf, tt_type);

	printf(" Hub Port Status:\n");
	for (i = 0; i < buf[2]; i++) {
		unsigned char status[4];

		ret = usb_control_msg(fd,
				LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS
					| LIBUSB_RECIPIENT_OTHER,
				LIBUSB_REQUEST_GET_STATUS,
				0, i + 1,
				status, sizeof status,
				CTRL_TIMEOUT);
		if (ret < 0) {
			fprintf(stderr,
				"cannot read port %d status, %s (%d)\n",
				i + 1, strerror(errno), errno);
			break;
		}

		printf("   Port %d: %02x%02x.%02x%02x", i + 1,
			status[3], status[2],
			status[1], status[0]);
		/* CAPS are used to highlight "transient" states */
		if (speed != 0x0300) {
			printf("%s%s%s%s%s",
					(status[2] & 0x10) ? " C_RESET" : "",
					(status[2] & 0x08) ? " C_OC" : "",
					(status[2] & 0x04) ? " C_SUSPEND" : "",
					(status[2] & 0x02) ? " C_ENABLE" : "",
					(status[2] & 0x01) ? " C_CONNECT" : "");
			printf("%s%s%s%s%s%s%s%s%s%s%s\n",
					(status[1] & 0x10) ? " indicator" : "",
					(status[1] & 0x08) ? " test" : "",
					(status[1] & 0x04) ? " highspeed" : "",
					(status[1] & 0x02) ? " lowspeed" : "",
					(status[1] & 0x01) ? " power" : "",
					(status[0] & 0x20) ? " L1" : "",
					(status[0] & 0x10) ? " RESET" : "",
					(status[0] & 0x08) ? " oc" : "",
					(status[0] & 0x04) ? " suspend" : "",
					(status[0] & 0x02) ? " enable" : "",
					(status[0] & 0x01) ? " connect" : "");
		} else {
			link_state = ((status[0] & 0xe0) >> 5) +
				((status[1] & 0x1) << 3);
			printf("%s%s%s%s%s%s",
					(status[2] & 0x80) ? " C_CONFIG_ERROR" : "",
					(status[2] & 0x40) ? " C_LINK_STATE" : "",
					(status[2] & 0x20) ? " C_BH_RESET" : "",
					(status[2] & 0x10) ? " C_RESET" : "",
					(status[2] & 0x08) ? " C_OC" : "",
					(status[2] & 0x01) ? " C_CONNECT" : "");
			printf("%s%s",
					((status[1] & 0x1C) == 0) ? " 5Gbps" : " Unknown Speed",
					(status[1] & 0x02) ? " power" : "");
			/* Link state is bits 8:5 */
			if (link_state < (sizeof(link_state_descriptions) /
						sizeof(*link_state_descriptions)))
				printf("%s", link_state_descriptions[link_state]);
			printf("%s%s%s%s\n",
					(status[0] & 0x10) ? " RESET" : "",
					(status[0] & 0x08) ? " oc" : "",
					(status[0] & 0x02) ? " enable" : "",
					(status[0] & 0x01) ? " connect" : "");
		}
	}
}

static void do_dualspeed(libusb_device_handle *fd)
{
	unsigned char buf[10];
	char cls[128], subcls[128], proto[128];
	int ret;

	ret = usb_control_msg(fd,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			USB_DT_DEVICE_QUALIFIER << 8, 0,
			buf, sizeof buf, CTRL_TIMEOUT);
	if (ret < 0 && errno != EPIPE)
		perror("can't get device qualifier");

	/* all dual-speed devices have a qualifier */
	if (ret != sizeof buf
			|| buf[0] != ret
			|| buf[1] != USB_DT_DEVICE_QUALIFIER)
		return;

	get_class_string(cls, sizeof(cls),
			buf[4]);
	get_subclass_string(subcls, sizeof(subcls),
			buf[4], buf[5]);
	get_protocol_string(proto, sizeof(proto),
			buf[4], buf[5], buf[6]);
	printf("Device Qualifier (for other device speed):\n"
	       "  bLength             %5u\n"
	       "  bDescriptorType     %5u\n"
	       "  bcdUSB              %2x.%02x\n"
	       "  bDeviceClass        %5u %s\n"
	       "  bDeviceSubClass     %5u %s\n"
	       "  bDeviceProtocol     %5u %s\n"
	       "  bMaxPacketSize0     %5u\n"
	       "  bNumConfigurations  %5u\n",
	       buf[0], buf[1],
	       buf[3], buf[2],
	       buf[4], cls,
	       buf[5], subcls,
	       buf[6], proto,
	       buf[7], buf[8]);

	/* FIXME also show the OTHER_SPEED_CONFIG descriptors */
}

static void do_debug(libusb_device_handle *fd)
{
	unsigned char buf[4];
	int ret;

	ret = usb_control_msg(fd,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			USB_DT_DEBUG << 8, 0,
			buf, sizeof buf, CTRL_TIMEOUT);
	if (ret < 0 && errno != EPIPE)
		perror("can't get debug descriptor");

	/* some high speed devices are also "USB2 debug devices", meaning
	 * you can use them with some EHCI implementations as another kind
	 * of system debug channel:  like JTAG, RS232, or a console.
	 */
	if (ret != sizeof buf
			|| buf[0] != ret
			|| buf[1] != USB_DT_DEBUG)
		return;

	printf("Debug descriptor:\n"
	       "  bLength              %4u\n"
	       "  bDescriptorType      %4u\n"
	       "  bDebugInEndpoint     0x%02x\n"
	       "  bDebugOutEndpoint    0x%02x\n",
	       buf[0], buf[1],
	       buf[2], buf[3]);
}

static const unsigned char *find_otg(const unsigned char *buf, int buflen)
{
	if (!buf)
		return 0;
	while (buflen >= 3) {
		if (buf[0] == 3 && buf[1] == USB_DT_OTG)
			return buf;
		if (buf[0] > buflen)
			return 0;
		buflen -= buf[0];
		buf += buf[0];
	}
	return 0;
}

static int do_otg(struct libusb_config_descriptor *config)
{
	unsigned	i, k;
	int		j;
	const unsigned char	*desc;

	/* each config of an otg device has an OTG descriptor */
	desc = find_otg(config->extra, config->extra_length);
	for (i = 0; !desc && i < config->bNumInterfaces; i++) {
		const struct libusb_interface *intf;

		intf = &config->interface[i];
		for (j = 0; !desc && j < intf->num_altsetting; j++) {
			const struct libusb_interface_descriptor *alt;

			alt = &intf->altsetting[j];
			desc = find_otg(alt->extra, alt->extra_length);
			for (k = 0; !desc && k < alt->bNumEndpoints; k++) {
				const struct libusb_endpoint_descriptor *ep;

				ep = &alt->endpoint[k];
				desc = find_otg(ep->extra, ep->extra_length);
			}
		}
	}
	if (!desc)
		return 0;

	printf("OTG Descriptor:\n"
		"  bLength               %3u\n"
		"  bDescriptorType       %3u\n"
		"  bmAttributes         0x%02x\n"
		"%s%s",
		desc[0], desc[1], desc[2],
		(desc[2] & 0x01)
			? "    SRP (Session Request Protocol)\n" : "",
		(desc[2] & 0x02)
			? "    HNP (Host Negotiation Protocol)\n" : "");
	return 1;
}

static void
dump_device_status(libusb_device_handle *fd, int otg, int wireless, int super_speed)
{
	unsigned char status[8];
	int ret;

	ret = usb_control_msg(fd, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD
				| LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_STATUS,
			0, 0,
			status, 2,
			CTRL_TIMEOUT);
	if (ret < 0) {
		fprintf(stderr,
			"cannot read device status, %s (%d)\n",
			strerror(errno), errno);
		return;
	}

	printf("Device Status:     0x%02x%02x\n",
			status[1], status[0]);
	if (status[0] & (1 << 0))
		printf("  Self Powered\n");
	else
		printf("  (Bus Powered)\n");
	if (status[0] & (1 << 1))
		printf("  Remote Wakeup Enabled\n");
	if (status[0] & (1 << 2) && !super_speed) {
		/* for high speed devices */
		if (!wireless)
			printf("  Test Mode\n");
		/* for devices with Wireless USB support */
		else
			printf("  Battery Powered\n");
	}
	if (super_speed) {
		if (status[0] & (1 << 2))
			printf("  U1 Enabled\n");
		if (status[0] & (1 << 3))
			printf("  U2 Enabled\n");
		if (status[0] & (1 << 4))
			printf("  Latency Tolerance Messaging (LTM) Enabled\n");
	}
	/* if both HOST and DEVICE support OTG */
	if (otg) {
		if (status[0] & (1 << 3))
			printf("  HNP Enabled\n");
		if (status[0] & (1 << 4))
			printf("  HNP Capable\n");
		if (status[0] & (1 << 5))
			printf("  ALT port is HNP Capable\n");
	}
	/* for high speed devices with debug descriptors */
	if (status[0] & (1 << 6))
		printf("  Debug Mode\n");

	if (!wireless)
		return;

	/* Wireless USB exposes FIVE different types of device status,
	 * accessed by distinct wIndex values.
	 */
	ret = usb_control_msg(fd, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD
				| LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_STATUS,
			0, 1 /* wireless status */,
			status, 1,
			CTRL_TIMEOUT);
	if (ret < 0) {
		fprintf(stderr,
			"cannot read wireless %s, %s (%d)\n",
			"status",
			strerror(errno), errno);
		return;
	}
	printf("Wireless Status:     0x%02x\n", status[0]);
	if (status[0] & (1 << 0))
		printf("  TX Drp IE\n");
	if (status[0] & (1 << 1))
		printf("  Transmit Packet\n");
	if (status[0] & (1 << 2))
		printf("  Count Packets\n");
	if (status[0] & (1 << 3))
		printf("  Capture Packet\n");

	ret = usb_control_msg(fd, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD
				| LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_STATUS,
			0, 2 /* Channel Info */,
			status, 1,
			CTRL_TIMEOUT);
	if (ret < 0) {
		fprintf(stderr,
			"cannot read wireless %s, %s (%d)\n",
			"channel info",
			strerror(errno), errno);
		return;
	}
	printf("Channel Info:        0x%02x\n", status[0]);

	/* 3=Received data: many bytes, for count packets or capture packet */

	ret = usb_control_msg(fd, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD
				| LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_STATUS,
			0, 3 /* MAS Availability */,
			status, 8,
			CTRL_TIMEOUT);
	if (ret < 0) {
		fprintf(stderr,
			"cannot read wireless %s, %s (%d)\n",
			"MAS info",
			strerror(errno), errno);
		return;
	}
	printf("MAS Availability:    ");
	dump_bytes(status, 8);

	ret = usb_control_msg(fd, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD
				| LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_STATUS,
			0, 5 /* Current Transmit Power */,
			status, 2,
			CTRL_TIMEOUT);
	if (ret < 0) {
		fprintf(stderr,
			"cannot read wireless %s, %s (%d)\n",
			"transmit power",
			strerror(errno), errno);
		return;
	}
	printf("Transmit Power:\n");
	printf(" TxNotification:     0x%02x\n", status[0]);
	printf(" TxBeacon:     :     0x%02x\n", status[1]);
}

static int do_wireless(libusb_device_handle *fd)
{
	/* FIXME fetch and dump BOS etc */
	if (fd)
		return 0;
	return 0;
}

static void dump_usb2_device_capability_desc(unsigned char *buf)
{
	unsigned int wide;

	wide = buf[3] + (buf[4] << 8) +
		(buf[5] << 16) + (buf[6] << 24);
	printf("  USB 2.0 Extension Device Capability:\n"
			"    bLength             %5u\n"
			"    bDescriptorType     %5u\n"
			"    bDevCapabilityType  %5u\n"
			"    bmAttributes   0x%08x\n",
			buf[0], buf[1], buf[2], wide);
	if (!(wide & 0x02))
		printf("      (Missing must-be-set LPM bit!)\n");
	else if (!(wide & 0x04))
		printf("      HIRD Link Power Management (LPM)"
				" Supported\n");
	else {
		printf("      BESL Link Power Management (LPM)"
				" Supported\n");
		if (wide & 0x08)
			printf("    BESL value    %5u us \n", wide & 0xf00);
		if (wide & 0x10)
			printf("    Deep BESL value    %5u us \n",
					wide & 0xf000);
	}
}

static void dump_ss_device_capability_desc(unsigned char *buf)
{
	if (buf[0] < 10) {
		printf("  Bad SuperSpeed USB Device Capability descriptor.\n");
		return;
	}
	printf("  SuperSpeed USB Device Capability:\n"
			"    bLength             %5u\n"
			"    bDescriptorType     %5u\n"
			"    bDevCapabilityType  %5u\n"
			"    bmAttributes         0x%02x\n",
			buf[0], buf[1], buf[2], buf[3]);
	if (buf[3] & 0x02)
		printf("      Latency Tolerance Messages (LTM)"
				" Supported\n");
	printf("    wSpeedsSupported   0x%02x%02x\n", buf[5], buf[4]);
	if (buf[4] & (1 << 0))
		printf("      Device can operate at Low Speed (1Mbps)\n");
	if (buf[4] & (1 << 1))
		printf("      Device can operate at Full Speed (12Mbps)\n");
	if (buf[4] & (1 << 2))
		printf("      Device can operate at High Speed (480Mbps)\n");
	if (buf[4] & (1 << 3))
		printf("      Device can operate at SuperSpeed (5Gbps)\n");

	printf("    bFunctionalitySupport %3u\n", buf[6]);
	switch(buf[6]) {
	case 0:
		printf("      Lowest fully-functional device speed is "
				"Low Speed (1Mbps)\n");
		break;
	case 1:
		printf("      Lowest fully-functional device speed is "
				"Full Speed (12Mbps)\n");
		break;
	case 2:
		printf("      Lowest fully-functional device speed is "
				"High Speed (480Mbps)\n");
		break;
	case 3:
		printf("      Lowest fully-functional device speed is "
				"SuperSpeed (5Gbps)\n");
		break;
	default:
		printf("      Lowest fully-functional device speed is "
				"at an unknown speed!\n");
		break;
	}
	printf("    bU1DevExitLat        %4u micro seconds\n", buf[7]);
	printf("    bU2DevExitLat    %8u micro seconds\n", buf[8] + (buf[9] << 8));
}

static void dump_container_id_device_capability_desc(unsigned char *buf)
{
	if (buf[0] < 20) {
		printf("  Bad Container ID Device Capability descriptor.\n");
		return;
	}
	printf("  Container ID Device Capability:\n"
			"    bLength             %5u\n"
			"    bDescriptorType     %5u\n"
			"    bDevCapabilityType  %5u\n"
			"    bReserved           %5u\n",
			buf[0], buf[1], buf[2], buf[3]);
	printf("    ContainerID             %s\n",
			get_guid(&buf[4]));
}

static void dump_bos_descriptor(libusb_device_handle *fd)
{
	/* Total for all known BOS descriptors is 43 bytes:
	 * 6 bytes for Wireless USB, 7 bytes for USB 2.0 extension,
	 * 10 bytes for SuperSpeed, 20 bytes for Container ID.
	 */
	unsigned char bos_desc[43];
	unsigned int bos_desc_size;
	int size, ret;
	unsigned char *buf;

	/* Get the first 5 bytes to get the wTotalLength field */
	ret = usb_control_msg(fd,
			LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			USB_DT_BOS << 8, 0,
			bos_desc, 5, CTRL_TIMEOUT);
	if (ret <= 0)
		return;
	else if (bos_desc[0] != 5 || bos_desc[1] != USB_DT_BOS)
		return;

	bos_desc_size = bos_desc[2] + (bos_desc[3] << 8);
	printf("Binary Object Store Descriptor:\n"
	       "  bLength             %5u\n"
	       "  bDescriptorType     %5u\n"
	       "  wTotalLength        %5u\n"
	       "  bNumDeviceCaps      %5u\n",
	       bos_desc[0], bos_desc[1],
	       bos_desc_size, bos_desc[4]);

	if (bos_desc_size <= 5) {
		if (bos_desc[4] > 0)
			fprintf(stderr, "Couldn't get "
					"device capability descriptors\n");
		return;
	}
	if (bos_desc_size > sizeof bos_desc) {
		fprintf(stderr, "FIXME: alloc bigger buffer for "
				"device capability descriptors\n");
		return;
	}

	ret = usb_control_msg(fd,
			LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_DEVICE,
			LIBUSB_REQUEST_GET_DESCRIPTOR,
			USB_DT_BOS << 8, 0,
			bos_desc, bos_desc_size, CTRL_TIMEOUT);
	if (ret < 0) {
		fprintf(stderr, "Couldn't get device capability descriptors\n");
		return;
	}

	size = bos_desc_size - 5;
	buf = &bos_desc[5];

	while (size >= 3) {
		if (buf[0] < 3) {
			printf("buf[0] = %u\n", buf[0]);
			return;
		}
		switch (buf[2]) {
		case USB_DC_WIRELESS_USB:
			/* FIXME */
			break;
		case USB_DC_20_EXTENSION:
			dump_usb2_device_capability_desc(buf);
			break;
		case USB_DC_SUPERSPEED:
			dump_ss_device_capability_desc(buf);
			break;
		case USB_DC_CONTAINER_ID:
			dump_container_id_device_capability_desc(buf);
			break;
		default:
			printf("  ** UNRECOGNIZED: ");
			dump_bytes(buf, buf[0]);
			break;
		}
		size -= buf[0];
		buf += buf[0];
	}
}

static void dumpdev(libusb_device *dev)
{
	libusb_device_handle *udev;
	struct libusb_device_descriptor desc;
	int i, ret;
	int otg, wireless;

	otg = wireless = 0;
	ret = libusb_open(dev, &udev);
	if (ret) {
		fprintf(stderr, "Couldn't open device, some information "
			"will be missing\n");
		udev = NULL;
	}

	libusb_get_device_descriptor(dev, &desc);
	dump_device(udev, &desc);
	if (desc.bcdUSB == 0x0250)
		wireless = do_wireless(udev);
	if (desc.bNumConfigurations) {
		struct libusb_config_descriptor *config;

		ret = libusb_get_config_descriptor(dev, 0, &config);
		if (ret) {
			fprintf(stderr, "Couldn't get configuration descriptor 0, "
					"some information will be missing\n");
		} else {
			otg = do_otg(config) || otg;
			libusb_free_config_descriptor(config);
		}

		for (i = 0; i < desc.bNumConfigurations; ++i) {
			ret = libusb_get_config_descriptor(dev, i, &config);
			if (ret) {
				fprintf(stderr, "Couldn't get configuration "
						"descriptor %d, some information will "
						"be missing\n", i);
			} else {
				dump_config(udev, config);
				libusb_free_config_descriptor(config);
			}
		}
	}
	if (!udev)
		return;

	if (desc.bDeviceClass == LIBUSB_CLASS_HUB)
		do_hub(udev, desc.bDeviceProtocol, desc.bcdUSB);
	if (desc.bcdUSB >= 0x0201) {
		dump_bos_descriptor(udev);
	}
	if (desc.bcdUSB == 0x0200) {
		do_dualspeed(udev);
	}
	do_debug(udev);
	dump_device_status(udev, otg, wireless, desc.bcdUSB >= 0x0300);
	libusb_close(udev);
}

/* ---------------------------------------------------------------------- */

static int dump_one_device(libusb_context *ctx, const char *path)
{
	libusb_device *dev;
	struct libusb_device_descriptor desc;
	char vendor[128], product[128];

	dev = get_usb_device(ctx, path);
	if (!dev) {
		fprintf(stderr, "Cannot open %s\n", path);
		return 1;
	}
	libusb_get_device_descriptor(dev, &desc);
	get_vendor_string(vendor, sizeof(vendor), desc.idVendor);
	get_product_string(product, sizeof(product), desc.idVendor, desc.idProduct);
	printf("Device: ID %04x:%04x %s %s\n", desc.idVendor,
					       desc.idProduct,
					       vendor,
					       product);
	dumpdev(dev);
	return 0;
}

static int list_devices(libusb_context *ctx, int busnum, int devnum, int vendorid, int productid)
{
	libusb_device **list;
	struct libusb_device_descriptor desc;
	char vendor[128], product[128];
	int status;
	ssize_t num_devs, i;

	status = 1; /* 1 device not found, 0 device found */

	num_devs = libusb_get_device_list(ctx, &list);
	if (num_devs < 0)
		goto error;

	for (i = 0; i < num_devs; ++i) {
		libusb_device *dev = list[i];
		uint8_t bnum = libusb_get_bus_number(dev);
		uint8_t dnum = libusb_get_device_address(dev);

		if ((busnum != -1 && busnum != bnum) ||
		    (devnum != -1 && devnum != dnum))
			continue;
		libusb_get_device_descriptor(dev, &desc);
		if ((vendorid != -1 && vendorid != desc.idVendor) ||
		    (productid != -1 && productid != desc.idProduct))
			continue;
		status = 0;
		get_vendor_string(vendor, sizeof(vendor), desc.idVendor);
		get_product_string(product, sizeof(product),
				desc.idVendor, desc.idProduct);
		if (verblevel > 0)
			printf("\n");
		printf("Bus %03u Device %03u: ID %04x:%04x %s %s\n",
				bnum, dnum,
				desc.idVendor,
				desc.idProduct,
				vendor, product);
		if (verblevel > 0)
			dumpdev(dev);
	}

	libusb_free_device_list(list, 0);
error:
	return status;
}


/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{ "version", 0, 0, 'V' },
		{ "verbose", 0, 0, 'v' },
		{ "help", 0, 0, 'h' },
		{ "tree", 0, 0, 't' },
		{ 0, 0, 0, 0 }
	};
	libusb_context *ctx;
	int c, err = 0;
	unsigned int treemode = 0;
	int bus = -1, devnum = -1, vendor = -1, product = -1;
	const char *devdump = NULL;
	int help = 0;
	char *cp;
	int status;

	setlocale(LC_CTYPE, "");

	while ((c = getopt_long(argc, argv, "D:vtP:p:s:d:Vh",
			long_options, NULL)) != EOF) {
		switch (c) {
		case 'V':
			printf("lsusb (" PACKAGE ") " VERSION "\n");
			return EXIT_SUCCESS;
		case 'v':
			verblevel++;
			break;

		case 'h':
			help=1;
			break;

		case 't':
			treemode = 1;
			break;

		case 's':
			cp = strchr(optarg, ':');
			if (cp) {
				*cp++ = 0;
				if (*optarg)
					bus = strtoul(optarg, NULL, 10);
				if (*cp)
					devnum = strtoul(cp, NULL, 10);
			} else {
				if (*optarg)
					devnum = strtoul(optarg, NULL, 10);
			}
			break;

		case 'd':
			cp = strchr(optarg, ':');
			if (!cp) {
				err++;
				break;
			}
			*cp++ = 0;
			if (*optarg)
				vendor = strtoul(optarg, NULL, 16);
			if (*cp)
				product = strtoul(cp, NULL, 16);
			break;

		case 'D':
			devdump = optarg;
			break;

		case '?':
		default:
			err++;
			break;
		}
	}
	if (err || argc > optind || help) {
		fprintf(stderr, "Usage: lsusb [options]...\n"
			"List USB devices\n"
			"  -v, --verbose\n"
			"      Increase verbosity (show descriptors)\n"
			"  -s [[bus]:][devnum]\n"
			"      Show only devices with specified device and/or\n"
			"      bus numbers (in decimal)\n"
			"  -d vendor:[product]\n"
			"      Show only devices with the specified vendor and\n"
			"      product ID numbers (in hexadecimal)\n"
			"  -D device\n"
			"      Selects which device lsusb will examine\n"
			"  -t, --tree\n"
			"      Dump the physical USB device hierarchy as a tree\n"
			"  -V, --version\n"
			"      Show version of program\n"
			"  -h, --help\n"
			"      Show usage and help\n"
			);
		return EXIT_FAILURE;
	}


	/* by default, print names as well as numbers */
	if (names_init() < 0)
		fprintf(stderr, "unable to initialize usb spec");

	status = 0;

	if (treemode) {
		/* treemode requires at least verblevel 1 */
		verblevel += 1 - VERBLEVEL_DEFAULT;
		status = lsusb_t();
		names_exit();
		return status;
	}

	err = libusb_init(&ctx);
	if (err) {
		fprintf(stderr, "unable to initialize libusb: %i\n", err);
		return EXIT_FAILURE;
	}

	if (devdump)
		status = dump_one_device(ctx, devdump);
	else
		status = list_devices(ctx, bus, devnum, vendor, product);

	names_exit();
	libusb_exit(ctx);
	return status;
}

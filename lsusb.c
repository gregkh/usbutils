/*****************************************************************************/

/*
 *      lsusb.c  --  lspci like utility for the USB bus
 *
 *      Copyright (C) 1999, 2000, 2001, 2003
 *        Thomas Sailer (t.sailer@alumni.ethz.ch)
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 */

/*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <usb.h>

#include "names.h"
#include "devtree.h"
#include "usbmisc.h"

#define _GNU_SOURCE
#include <getopt.h>


/* from USB 2.0 spec and updates */
#define USB_DT_DEVICE_QUALIFIER		0x06
#define USB_DT_OTHER_SPEED_CONFIG	0x07
#define USB_DT_OTG			0x09
#define USB_DT_DEBUG			0x0a
#define USB_DT_INTERFACE_ASSOCIATION	0x0b

/* convention suggested by common class spec */
#define USB_DT_CS_INTERFACE		0x24
#define USB_DT_CS_ENDPOINT		0x25

#define VERBLEVEL_DEFAULT 0	/* 0 gives lspci behaviour; 1, lsusb-0.9 */

#define CTRL_RETRIES	 2
#define CTRL_TIMEOUT	(5*1000)	/* milliseconds */

static const char *procbususb = "/proc/bus/usb";
static unsigned int verblevel = VERBLEVEL_DEFAULT;
static int do_report_desc = 1;

static void dump_interface(struct usb_dev_handle *dev, struct usb_interface *interface);
static void dump_endpoint(struct usb_dev_handle *dev, struct usb_interface_descriptor *interface, struct usb_endpoint_descriptor *endpoint);
static void dump_audiocontrol_interface(struct usb_dev_handle *dev, unsigned char *buf);
static void dump_audiostreaming_interface(unsigned char *buf);
static void dump_midistreaming_interface(struct usb_dev_handle *dev, unsigned char *buf);
static char *dump_comm_descriptor(struct usb_dev_handle *dev, unsigned char *buf, char *indent);
static void dump_hid_device(struct usb_dev_handle *dev, struct usb_interface_descriptor *interface, unsigned char *buf);
static void dump_audiostreaming_endpoint(unsigned char *buf);
static void dump_midistreaming_endpoint(unsigned char *buf);
static void dump_hub(char *prefix, unsigned char *p, int has_tt);
static void dump_ccid_device(unsigned char *buf);

/* ---------------------------------------------------------------------- */

static unsigned int convert_le_u32 (const unsigned char *buf)
{
        return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24); 
}

/* ---------------------------------------------------------------------- */

/* workaround libusb API goofs:  "byte" should never be sign extended;
 * using "char" is trouble.  Likewise, sizes should never be negative.
 */

static inline int typesafe_control_msg(usb_dev_handle *dev,
	unsigned char requesttype, unsigned char request,
	int value, int index,
	unsigned char *bytes, unsigned size, int timeout)
{
	return usb_control_msg(dev, requesttype, request, value, index,
		(char *) bytes, (int) size, timeout);
}

#define usb_control_msg		typesafe_control_msg

/* ---------------------------------------------------------------------- */

int lprintf(unsigned int vl, const char *format, ...)
{
        va_list ap;
        int r;

        if (vl > verblevel)
                return 0;
        va_start(ap, format);
	r = vfprintf(stderr, format, ap);
        va_end(ap);
        if (!vl)
                exit(1);
        return r;
}

/* ---------------------------------------------------------------------- */

static int get_string(struct usb_dev_handle *dev, char* buf, size_t size, u_int8_t id)
{
	int ret;
	
	if (id) {
		ret = usb_get_string_simple(dev, id, buf, size);
		if (ret <= 0) {
			buf[0] = 0;
			return 0;
		}
		else
			return ret;
		
	} 
	else {
		buf[0] = 0;
		return 0;
	}
}

static int get_vendor_string(char *buf, size_t size, u_int16_t vid)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_vendor(vid)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

static int get_product_string(char *buf, size_t size, u_int16_t vid, u_int16_t pid)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_product(vid, pid)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

static int get_class_string(char *buf, size_t size, u_int8_t cls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_class(cls)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

static int get_subclass_string(char *buf, size_t size, u_int8_t cls, u_int8_t subcls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_subclass(cls, subcls)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

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

/* ---------------------------------------------------------------------- */

static void dump_bytes(unsigned char *buf, unsigned int len)
{
	unsigned int i;

	for (i = 0; i < len; i++)
		printf(" %02x", buf[i]);
	printf("\n");
}

static void dump_junk(unsigned char *buf, const char *indent, unsigned int len)
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
	struct usb_dev_handle *dev,
	struct usb_device_descriptor *descriptor
)
{
	char vendor[128], product[128];
	char cls[128], subcls[128], proto[128];
	char mfg[128], prod[128], serial[128];

	get_vendor_string(vendor, sizeof(vendor), descriptor->idVendor);
	get_product_string(product, sizeof(product), descriptor->idVendor, descriptor->idProduct);
	get_class_string(cls, sizeof(cls), descriptor->bDeviceClass);
	get_subclass_string(subcls, sizeof(subcls), descriptor->bDeviceClass, descriptor->bDeviceSubClass);
	get_protocol_string(proto, sizeof(proto), descriptor->bDeviceClass, descriptor->bDeviceSubClass, descriptor->bDeviceProtocol);
	get_string(dev, mfg, sizeof(mfg), descriptor->iManufacturer);
	get_string(dev, prod, sizeof(prod), descriptor->iProduct);
	get_string(dev, serial, sizeof(serial), descriptor->iSerialNumber);
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
	       descriptor->bLength, descriptor->bDescriptorType, descriptor->bcdUSB >> 8, descriptor->bcdUSB & 0xff, 
	       descriptor->bDeviceClass, cls, descriptor->bDeviceSubClass, subcls, descriptor->bDeviceProtocol, proto, 
	       descriptor->bMaxPacketSize0, descriptor->idVendor, vendor, descriptor->idProduct, product, 
	       descriptor->bcdDevice >> 8, descriptor->bcdDevice & 0xff, descriptor->iManufacturer, mfg,
	       descriptor->iProduct, prod, descriptor->iSerialNumber, serial, descriptor->bNumConfigurations);
}

static void dump_config(struct usb_dev_handle *dev, struct usb_config_descriptor *config)
{
	char cfg[128];
	int i;
	
	get_string(dev, cfg, sizeof(cfg), config->iConfiguration);
	printf("  Configuration Descriptor:\n"
	       "    bLength             %5u\n"
	       "    bDescriptorType     %5u\n"
	       "    wTotalLength        %5u\n"
	       "    bNumInterfaces      %5u\n"
	       "    bConfigurationValue %5u\n"
	       "    iConfiguration      %5u %s\n"
	       "    bmAttributes         0x%02x\n",
	       config->bLength, config->bDescriptorType, config->wTotalLength,
	       config->bNumInterfaces, config->bConfigurationValue, config->iConfiguration,
	       cfg, config->bmAttributes);
	if (config->bmAttributes & 0x40)
		printf("      Self Powered\n");
	if (config->bmAttributes & 0x20)
		printf("      Remote Wakeup\n");
	printf("    MaxPower            %5umA\n", config->MaxPower * 2);

	/* avoid re-ordering or hiding descriptors for display */
	if (config->extralen) {
		int		size = config->extralen;
		unsigned char	*buf = config->extra;

		while (size >= 2) {
			if (buf[0] < 2) {
				dump_junk(buf, "        ", size);
				break;
			}
			switch (buf[1]) {
			case USB_DT_OTG:
				/* handled separately */
				break;
			default:
				/* often a misplaced class descriptor */
				printf("    UNRECOGNIZED: ");
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

static void dump_altsetting(struct usb_dev_handle *dev, struct usb_interface_descriptor *interface)
{
	char cls[128], subcls[128], proto[128];
	char ifstr[128];
	
	unsigned char *buf;
	unsigned size, i;

	get_class_string(cls, sizeof(cls), interface->bInterfaceClass);
	get_subclass_string(subcls, sizeof(subcls), interface->bInterfaceClass, interface->bInterfaceSubClass);
	get_protocol_string(proto, sizeof(proto), interface->bInterfaceClass, interface->bInterfaceSubClass, interface->bInterfaceProtocol);
	get_string(dev, ifstr, sizeof(ifstr), interface->iInterface);
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

	/* avoid re-ordering or hiding descriptors for display */
	if (interface->extralen)
	{
		size = interface->extralen;
		buf = interface->extra;
		while (size >= 2 * sizeof(u_int8_t))
		{
			if (buf[0] < 2) {
				dump_junk(buf, "      ", size);
				break;
			} 
			switch (buf[1]) {
			case USB_DT_CS_INTERFACE:
				switch(interface->bInterfaceClass) {
					case USB_CLASS_AUDIO:
						switch(interface->bInterfaceSubClass) {
							case 1:
								dump_audiocontrol_interface(dev, buf);
								break;
							case 2:
								dump_audiostreaming_interface(buf);
								break;
							case 3:
								dump_midistreaming_interface(dev, buf);
								break;
						}
						break;
					case USB_CLASS_COMM:
						dump_comm_descriptor(dev, buf,
							"      ");
						break;
				}
				break;
			case USB_DT_HID:
				if (interface->bInterfaceClass == USB_CLASS_HID)
					dump_hid_device(dev, interface, buf);
				if (interface->bInterfaceClass == 0x0b)
					dump_ccid_device(buf);
				break;
			case USB_DT_OTG:
				/* handled separately */
				break;
			default:
				/* often a misplaced class descriptor */
				printf("      UNRECOGNIZED: ");
				dump_bytes(buf, buf[0]);
				break;
			}
			size -= buf[0];
			buf += buf[0];
		}
	}
	for (i = 0 ; i < interface->bNumEndpoints ; i++)
		dump_endpoint(dev, interface, &interface->endpoint[i]);
}

static void dump_interface(struct usb_dev_handle *dev, struct usb_interface *interface)
{
	int i;
	
	for (i = 0; i < interface->num_altsetting; i++)
		dump_altsetting(dev, &interface->altsetting[i]);
}

static void dump_endpoint(struct usb_dev_handle *dev, struct usb_interface_descriptor *interface, struct usb_endpoint_descriptor *endpoint)
{
	static const char *typeattr[] = { "Control", "Isochronous", "Bulk", "Interrupt" };
	static const char *syncattr[] = { "None", "Asynchronous", "Adaptive", "Synchronous" };
	static const char *usage[] = { "Data", "Feedback", "Implicit feedback Data", "(reserved)" };
	static const char *hb[] = { "1x", "2x", "3x", "(?\?)" };
	unsigned char *buf;
	unsigned size;

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
	       endpoint->bLength, endpoint->bDescriptorType, endpoint->bEndpointAddress, endpoint->bEndpointAddress & 0x0f, 
	       (endpoint->bEndpointAddress & 0x80) ? "IN" : "OUT", endpoint->bmAttributes, 
	       typeattr[endpoint->bmAttributes & 3], syncattr[(endpoint->bmAttributes >> 2) & 3], 
	       usage[(endpoint->bmAttributes >> 4) & 3], endpoint->wMaxPacketSize,
	       hb[(endpoint->wMaxPacketSize >> 11) & 3],
	       endpoint->wMaxPacketSize & 0x3ff,
	       endpoint->bInterval);
	/* only for audio endpoints */
	if (endpoint->bLength == 9) 
	printf("        bRefresh            %5u\n"
	       "        bSynchAddress       %5u\n",
	       endpoint->bRefresh, endpoint->bSynchAddress);
	
	/* avoid re-ordering or hiding descriptors for display */
	if (endpoint->extralen)
	{
		size = endpoint->extralen;
		buf = endpoint->extra;
		while (size >= 2 * sizeof(u_int8_t))
		{
			if (buf[0] < 2) {
				dump_junk(buf, "        ", size);
				break;
			}
			switch (buf[1]) {
			case USB_DT_CS_ENDPOINT:
				if (interface->bInterfaceClass == 1 && interface->bInterfaceSubClass == 2)
					dump_audiostreaming_endpoint(buf);
				else if (interface->bInterfaceClass == 1 && interface->bInterfaceSubClass == 3)
					dump_midistreaming_endpoint(buf);
				break;
			case USB_DT_CS_INTERFACE:
				/* MISPLACED DESCRIPTOR ... less indent */
				switch (interface->bInterfaceClass) {
				case USB_CLASS_COMM:
				case USB_CLASS_DATA:	// comm data
					dump_comm_descriptor(dev, buf,
						"      ");
					break;
				default:
					printf("        INTERFACE CLASS: ");
					dump_bytes(buf, buf[0]);
				}
				break;
			case USB_DT_OTG:
				/* handled separately */
				break;
			default:
				/* often a misplaced class descriptor */
				printf("        UNRECOGNIZED: ");
				dump_bytes(buf, buf[0]);
				break;
			}
			size -= buf[0];
			buf += buf[0];
		}
	}
}

/* ---------------------------------------------------------------------- */

/*
 * Audio Class descriptor dump
 */

static void dump_audiocontrol_interface(struct usb_dev_handle *dev, unsigned char *buf)
{
	static const char *chconfig[] = {
		"Left Front (L)", "Right Front (R)", "Center Front (C)", "Low Freqency Enhancement (LFE)",
		"Left Surround (LS)", "Right Surround (RS)", "Left of Center (LC)", "Right of Center (RC)",
		"Surround (S)", "Side Left (SL)", "Side Right (SR)", "Top (T)"
	};
	static const char *chftrcontrols[] = {
		"Mute", "Volume", "Bass", "Mid", "Treble", "Graphic Equalizer", "Automatic Gain", "Delay", "Bass Boost", "Loudness"
	};
	unsigned int i, chcfg, j, k, N, termt;
	char chnames[128], term[128], termts[128];

	if (buf[1] != USB_DT_CS_INTERFACE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 3)
		printf("      Warning: Descriptor too short\n");
	printf("      AudioControl Interface Descriptor:\n"
	       "        bLength             %5u\n"
	       "        bDescriptorType     %5u\n"
	       "        bDescriptorSubtype  %5u ",
	       buf[0], buf[1], buf[2]);
	switch (buf[2]) {
	case 0x01:  /* HEADER */
		printf("(HEADER)\n");
		if (buf[0] < 8+buf[7])
			printf("      Warning: Descriptor too short\n");
		printf("        bcdADC              %2x.%02x\n"
		       "        wTotalLength        %5u\n"
		       "        bInCollection       %5u\n",
		       buf[4], buf[3], buf[5] | (buf[6] << 8), buf[7]);
		for(i = 0; i < buf[7]; i++)
			printf("        baInterfaceNr(%2u)   %5u\n", i, buf[8+i]);
		dump_junk(buf, "        ", 8+buf[7]);
		break;

	case 0x02:  /* INPUT_TERMINAL */
		printf("(INPUT_TERMINAL)\n");
		get_string(dev, chnames, sizeof(chnames), buf[10]);
		get_string(dev, term, sizeof(term), buf[11]);
		termt = buf[4] | (buf[5] << 8);
		get_audioterminal_string(termts, sizeof(termts), termt);
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

	case 0x03:  /* OUTPUT_TERMINAL */
		printf("(OUTPUT_TERMINAL)\n");
		get_string(dev, term, sizeof(term), buf[8]);
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

	case 0x04:  /* MIXER_UNIT */
		printf("(MIXER_UNIT)\n");
		j = buf[4];
		k = buf[j+5];
		if (j == 0 || k == 0) { 
		  printf("      Warning: mixer with %5u input and %5u output channels.\n", j, k);
		  N = 0;
		} else {
		  N = 1+(j*k-1)/8;
		}
		get_string(dev, chnames, sizeof(chnames), buf[8+j]);
		get_string(dev, term, sizeof(term), buf[9+j+N]);
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

	case 0x05:  /* SELECTOR_UNIT */
		printf("(SELECTOR_UNIT)\n");
		if (buf[0] < 6+buf[4])
			printf("      Warning: Descriptor too short\n");
		get_string(dev, term, sizeof(term), buf[5+buf[4]]);

		printf("        bUnitID             %5u\n"
		       "        bNrInPins           %5u\n",
		       buf[3], buf[4]);
		for (i = 0; i < buf[4]; i++)
			printf("        baSource(%2u)        %5u\n", i, buf[5+i]);
		printf("        iSelector           %5u %s\n",
		       buf[5+buf[4]], term);
		dump_junk(buf, "        ", 6+buf[4]);
		break;

	case 0x06:  /* FEATURE_UNIT */
		printf("(FEATURE_UNIT)\n");
		j = buf[5];
		if (!j)
			j = 1;
		k = (buf[0] - 7) / j;
		if (buf[0] < 7+buf[5]*k)
			printf("      Warning: Descriptor too short\n");
		get_string(dev, term, sizeof(term), buf[6+buf[5]*k]);
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
			for (j = 0; j < 10; j++)
				if ((chcfg >> j) & 1)
					printf("          %s\n", chftrcontrols[j]);
		}
		printf("        iFeature            %5u %s\n", buf[6+buf[5]*k], term);
		dump_junk(buf, "        ", 7+buf[5]*k);
		break;

	case 0x07:  /* PROCESSING_UNIT */
		printf("(PROCESSING_UNIT)\n");
		j = buf[6];
		k = buf[11+j];
		get_string(dev, chnames, sizeof(chnames), buf[10+j]);
		get_string(dev, term, sizeof(term), buf[12+j+k]);
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

	case 0x08:  /* EXTENSION_UNIT */
		printf("(EXTENSION_UNIT)\n");
		j = buf[6];
		k = buf[11+j];
		get_string(dev, chnames, sizeof(chnames), buf[10+j]);
		get_string(dev, term, sizeof(term), buf[12+j+k]);
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

	default:
		printf("(unknown)\n"
		       "        Invalid desc subtype:");
		dump_bytes(buf+3, buf[0]-3);
		break;
	}
}

static void dump_audiostreaming_interface(unsigned char *buf)
{
	static const char *fmtItag[] = { "TYPE_I_UNDEFINED", "PCM", "PCM8", "IEEE_FLOAT", "ALAW", "MULAW" };
	static const char *fmtIItag[] = { "TYPE_II_UNDEFINED", "MPEG", "AC-3" };
	static const char *fmtIIItag[] = { "TYPE_III_UNDEFINED", "IEC1937_AC-3", "IEC1937_MPEG-1_Layer1", 
					   "IEC1937_MPEG-Layer2/3/NOEXT", "IEC1937_MPEG-2_EXT",
					   "IEC1937_MPEG-2_Layer1_LS", "IEC1937_MPEG-2_Layer2/3_LS" };
	unsigned int i, j, fmttag;
	const char *fmtptr = "undefined";

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

	case 0x02: /* FORMAT_TYPE */
		printf("(FORMAT_TYPE)\n");
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
			switch ((buf[7] << 4) & 3) {
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
			if (buf[9] & 0x0)
				printf("          RF mode\n");
			if (buf[9] & 0x0)
				printf("          Line mode\n");
			if (buf[9] & 0x0)
				printf("          Custom0 mode\n");
			if (buf[9] & 0x0)
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
}

static void dump_audiostreaming_endpoint(unsigned char *buf)
{
	static const char *lockdelunits[] = { "Undefined", "Milliseconds", "Decoded PCM samples", "Reserved" };
	unsigned int lckdelidx;

	if (buf[1] != USB_DT_CS_ENDPOINT)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 7)
		printf("      Warning: Descriptor too short\n");
	printf("        AudioControl Endpoint Descriptor:\n"
	       "          bLength             %5u\n"
	       "          bDescriptorType     %5u\n"
	       "          bDescriptorSubtype  %5u (%s)\n"
	       "          bmAttributes         0x%02x\n",
	       buf[0], buf[1], buf[2], buf[2] == 1 ? "EP_GENERAL" : "invalid", buf[3]);
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
}

static void dump_midistreaming_interface(struct usb_dev_handle *dev, unsigned char *buf)
{
	static const char *jacktypes[] = {"Undefined", "Embedded", "External"};
	char jackstr[128];
	unsigned int j, tlength, capssize;
	unsigned long caps;

	if (buf[1] != USB_DT_CS_INTERFACE)
		printf("      Warning: Invalid descriptor\n");
	else if (buf[0] < 3)
		printf("      Warning: Descriptor too short\n");
	printf( "      MIDIStreaming Interface Descriptor:\n"
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
		printf( "        bcdADC              %2x.%02x\n"
			"        wTotalLength        %5u\n",
			buf[4], buf[3], tlength);
		dump_junk(buf, "        ", 7);
		break;

	case 0x02:
		printf("(MIDI_IN_JACK)\n");
		if (buf[0] < 6)
			printf("      Warning: Descriptor too short\n");
		get_string(dev, jackstr, sizeof(jackstr), buf[5]);
		printf( "        bJackType           %5u %s\n"
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
		printf( "        bJackType           %5u %s\n"
			"        bJackID             %5u\n"
			"        bNrInputPins        %5u\n",
			buf[3], buf[3] < 3 ? jacktypes[buf[3]] : "Invalid",
			buf[4], buf[5]);
		for (j=0; j < buf[5]; j++) {
			printf( "        baSourceID(%2u)      %5u\n"
				"        BaSourcePin(%2u)     %5u\n",
				j, buf[2*j+6], j, buf[2*j+7]);
		}
		j = 6+buf[5]*2; /* midi10.pdf says, incorrectly: 5+2*p */
		get_string(dev, jackstr, sizeof(jackstr), buf[j]);
		printf( "        iJack               %5u %s\n",
			buf[j], jackstr);
		dump_junk(buf, "        ", j+1);
		break;

	case 0x04:
		printf("(ELEMENT)\n");
		if (buf[0] < 12)
			printf("      Warning: Descriptor too short\n");
		printf( "        bElementID          %5u\n"
			"        bNrInputPins        %5u\n",
			buf[3], buf[4]);
		for(j=0; j < buf[4]; j++) {
		printf( "        baSourceID(%2u)      %5u\n"
			"        BaSourcePin(%2u)     %5u\n",
			j, buf[2*j+5], j, buf[2*j+6]);
		}
		j = 5+buf[4]*2;
		printf( "        bNrOutputPins       %5u\n"
			"        bInTerminalLink     %5u\n"
			"        bOutTerminalLink    %5u\n"
			"        bElCapsSize         %5u\n",
			buf[j], buf[j+1], buf[j+2], buf[j+3]);
		capssize = buf[j+3];
		caps = 0;
		for(j=0; j < capssize; j++) {
			caps |= (buf[j+9+buf[4]*2] << (8*j));
		}
		printf( "        bmElementCaps  0x%08lx\n", caps);
		if (caps & 0x01)
			printf( "          Undefined\n");
		if (caps & 0x02)
			printf( "          MIDI Clock\n");
		if (caps & 0x04)
			printf( "          MTC (MIDI Time Code)\n");
		if (caps & 0x08)
			printf( "          MMC (MIDI Machine Control)\n");
		if (caps & 0x10)
			printf( "          GM1 (General MIDI v.1)\n");
		if (caps & 0x20)
			printf( "          GM2 (General MIDI v.2)\n");
		if (caps & 0x40)
			printf( "          GS MIDI Extension\n");
		if (caps & 0x80)
			printf( "          XG MIDI Extension\n");
		if (caps & 0x100)
			printf( "          EFX\n");
		if (caps & 0x200)
			printf( "          MIDI Patch Bay\n");
		if (caps & 0x400)
			printf( "          DLS1 (Downloadable Sounds Level 1)\n");
		if (caps & 0x800)
			printf( "          DLS2 (Downloadable Sounds Level 2)\n");
		j = 9+2*buf[4]+capssize;
		get_string(dev, jackstr, sizeof(jackstr), buf[j]);
		printf( "        iElement            %5u %s\n", buf[j], jackstr);
		dump_junk(buf, "        ", j+1);
		break;

	default:
		printf("\n        Invalid desc subtype: ");
		dump_bytes(buf+3, buf[0]-3);
		break;
	}
}

static void dump_midistreaming_endpoint(unsigned char *buf)
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
	for (j=0; j<buf[3]; j++) {
		printf("          baAssocJackID(%2u)   %5u\n", j, buf[4+j]);
	}
	dump_junk(buf, "          ", 4+buf[3]);
}

static void dump_hub(char *prefix, unsigned char *p, int has_tt)
{
	unsigned int l, i, j;
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
	if (has_tt) {
		l = (wHubChar >> 5) & 0x03;
		printf("%s    TT think time %d FS bits\n", prefix, (l + 1) * 8);
	}
	if (wHubChar & (1<<7))
		printf("%s    Port indicators\n", prefix);
	printf("%s  bPwrOn2PwrGood      %3u * 2 milli seconds\n", prefix, p[5]);
	printf("%s  bHubContrCurrent    %3u milli Ampere\n", prefix, p[6]);
	l= (p[2] >> 3) + 1; /* this determines the variable number of bytes following */
	printf("%s  DeviceRemovable   ", prefix);
	for(i = 0; i < l; i++) 
		printf(" 0x%02x", p[7+i]);
	printf("\n%s  PortPwrCtrlMask   ", prefix);
 	for(j = 0; j < l; j++)
 		printf(" 0x%02x ", p[7+i+j]);
 	printf("\n");
}

static void dump_ccid_device(unsigned char *buf)
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
                "        bVoltageSupport     %5u  %s\n",
                buf[4],
                buf[5], (buf[5] == 1? "5.0V" : buf[5] == 2? "3.0V"
                         : buf[5] == 3? "1.8V":"?"));

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
		fputs("          Auto configuration based on ATR\n",stdout);
        if ((us & 0x0004))
		fputs("          Auto activation on insert\n",stdout);
        if ((us & 0x0008))
		fputs("          Auto voltage selection\n",stdout);
        if ((us & 0x0010))
		fputs("          Auto clock change\n",stdout);
        if ((us & 0x0020))
		fputs("          Auto baud rate change\n",stdout);
        if ((us & 0x0040))
		fputs("          Auto parameter negotation made by CCID\n",stdout);
        else if ((us & 0x0080))
		fputs("          Auto PPS made by CCID\n",stdout);
        else if ((us & (0x0040 | 0x0080)))
		fputs("        WARNING: conflicting negotation features\n",stdout);

        if ((us & 0x0100))
		fputs("          CCID can set ICC in clock stop mode\n",stdout);
        if ((us & 0x0200))
		fputs("          NAD value other than 0x00 accpeted\n",stdout);
        if ((us & 0x0400))
		fputs("          Auto IFSD exchange\n",stdout);

        if ((us & 0x00010000))
		fputs("          TPDU level exchange\n",stdout);
        else if ((us & 0x00020000))
		fputs("          Short APDU level exchange\n",stdout);
        else if ((us & 0x00040000))
		fputs("          Short and extended APDU level exchange\n",stdout);
        else if ((us & 0x00070000))
		fputs("        WARNING: conflicting exchange levels\n",stdout);

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
        unsigned int t, j, bsize, btag, btype, data = 0xffff, hut = 0xffff;
	int i;
	char *types[4] = { "Main", "Global", "Local", "reserved" };
	char indent[] = "                            ";

	printf("          Report Descriptor: (length is %d)\n", l);
	for(i = 0; i < l; ) {
		t = b[i];
		bsize = b[i] & 0x03;
		if (bsize == 3)
                        bsize = 4;
		btype = b[i] & (0x03 << 2);
		btag = b[i] & ~0x03; /* 2 LSB bits encode length */
		printf("            Item(%-6s): %s, data=", types[btype>>2], names_reporttag(btag));
		if (bsize > 0) {
			printf(" [ ");
			data = 0;
			for(j = 0; j < bsize; j++) {
				printf("0x%02x ", b[i+1+j]);
				data += (b[i+1+j] << (8*j));
			}
			printf("] %d", data);
		} else
                        printf("none");
		printf("\n");
		switch(btag) {
		case 0x04 : /* Usage Page */
                        printf("%s%s\n", indent, names_huts(data));
			hut = data;
                        break;
                        
		case 0x08 : /* Usage */
		case 0x18 : /* Usage Minimum */
		case 0x28 : /* Usage Maximum */
			printf("%s%s\n", indent, names_hutus((hut << 16) + data));
			break;
                        
		case 0xa0 : /* Collection */
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
                                
			default:
				if(data & 0x80)
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
			       data & 0x01 ? "Constant": "Data",
			       data & 0x02 ? "Variable": "Array",
			       data & 0x04 ? "Relative": "Absolute",
			       data & 0x08 ? "Wrap" : "No_Wrap",
			       data & 0x10 ? "Non_Linear": "Linear",
			       indent,
			       data & 0x20 ? "No_Preferred_State": "Preferred_State",
			       data & 0x40 ? "Null_State": "No_Null_Position",
			       data & 0x80 ? "Volatile": "Non_Volatile",
			       data &0x100 ? "Buffered Bytes": "Bitfield"
				);
		}
		i += 1 + bsize;
	}
}

static void dump_hid_device(struct usb_dev_handle *dev, struct usb_interface_descriptor *interface, unsigned char *buf)
{
	unsigned int i, len;
	int n;
	unsigned char dbuf[8192];

	if (buf[1] != USB_DT_HID)
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
	
	for (i = 0; i < buf[5]; i++) {
		/* we are just interested in report descriptors*/
		if (buf[6+3*i] != USB_DT_REPORT)
			continue;
		len = buf[7+3*i] | (buf[8+3*i] << 8);
		if (len > sizeof(dbuf)) {
			printf("report descriptor too long\n");
			continue;
		}
		if (usb_claim_interface(dev, interface->bInterfaceNumber) == 0) {
			if ((n = usb_control_msg(dev, 
					 USB_ENDPOINT_IN | USB_TYPE_STANDARD
					 	| USB_RECIP_INTERFACE, 
					 USB_REQ_GET_DESCRIPTOR, 
					 (USB_DT_REPORT << 8), 
					 interface->bInterfaceNumber,
					 dbuf, 
					 len, 
					 CTRL_TIMEOUT)) > 0)
				dump_report_desc(dbuf, n);
			usb_release_interface(dev, interface->bInterfaceNumber);
		} else {
			/* recent Linuxes require claim() for RECIP_INTERFACE,
			 * so "rmmod hid" will often make these available.
			 */
			printf( "         Report Descriptors: \n"
				"           ** UNAVAILABLE **\n");
		}
	}
}

static char *
dump_comm_descriptor(struct usb_dev_handle *dev, unsigned char *buf, char *indent)
{
	int		tmp;
	char		str [128];

	switch (buf[2]) {
	case 0:
		if (buf[0] != 5)
			goto bad;
		printf("%sCDC Header:\n"
		       "%s  bcdCDC               %x.%02x\n",
		       indent,
		       indent, buf[4], buf[3]);
		break;
	case 0x01:		/* call management functional desc */
		if (buf [0] != 5)
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
		if (buf [0] != 4)
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
	// case 0x03:		/* direct line management */
	// case 0x04:		/* telephone ringer */
	// case 0x05:		/* telephone call and line state reporting */
	case 0x06:		/* union desc */
		if (buf [0] < 5)
			goto bad;
		printf("%sCDC Union:\n"
		       "%s  bMasterInterface        %d\n"
		       "%s  bSlaveInterface         ",
		       indent,
		       indent, buf [3],
		       indent);
		for (tmp = 4; tmp < buf [0]; tmp++)
			printf("%d ", buf [tmp]);
		printf("\n");
		break;
	case 0x07:		/* country selection functional desc */
		if (buf [0] < 6 || (buf[0] & 1) != 0)
			goto bad;
		get_string(dev, str, sizeof str, buf[3]);
		printf("%sCountry Selection:\n"
		       "%s  iCountryCodeRelDate     %4d %s\n",
		       indent,
		       indent, buf[3], (buf[3] && *str) ? str : "(?\?)");
		for (tmp = 4; tmp < buf [0]; tmp += 2) {
			printf("%s  wCountryCode          0x%02x%02x\n",
				indent, buf[tmp], buf[tmp + 1]);
		}
		break;
	case 0x08:		/* telephone operational modes */
		if (buf [0] != 4)
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
	// case 0x09:		/* USB terminal */
	// case 0x0a:		/* network channel terminal */
	// case 0x0b:		/* protocol unit */
	// case 0x0c:		/* extension unit */
	// case 0x0d:		/* multi-channel management */
	// case 0x0e:		/* CAPI control management*/
	case 0x0f:		/* ethernet functional desc */
		if (buf [0] != 13)
			goto bad;
		get_string(dev, str, sizeof str, buf[3]);
		tmp = buf [7] << 8;
		tmp |= buf [6]; tmp <<= 8;
		tmp |= buf [5]; tmp <<= 8;
		tmp |= buf [4];
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
	// case 0x10:		/* ATM networking */
	case 0x11:		/* WHCM functional desc */
		if (buf[0] != 5)
			goto bad;
		printf("%sCDC WHCM:\n"
		       "%s  bcdVersion           %x.%02x\n",
		       indent,
		       indent, buf[4], buf[3]);
		break;
	case 0x12:		/* MDLM functional desc */
		if (buf [0] != 21)
			goto bad;
		printf("%sCDC MDLM:\n"
		       "%s  bcdCDC               %x.%02x\n"
		       "%s  bGUID               ",
		       indent,
		       indent, buf[4], buf[3],
		       indent);
		/* NOTE there are more traditional byteswap aware UUID-printing
		 * schemes, something like X0X1X2X3-X4X5-X6X7-X8X9-XaXbXcXdXeXf
		 * (or maybe it's X3X2X1X0-X5X4-...?) would be better
		 */
		dump_bytes(buf + 5, 16);
		break;
	case 0x13:		/* MDLM detail desc */
		if (buf [0] < 5)
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
		if (buf[0] != 7)
			goto bad;
		printf("%sCDC Device Management:\n"
		       "%s  bcdVersion           %x.%02x\n"
		       "%s  wMaxCommand          %d\n",
		       indent,
		       indent, buf[4], buf[3],
		       indent, (buf[6]<<8)| buf[5]);
		break;
	case 0x15:		/* OBEX functional desc */
		if (buf[0] != 5)
			goto bad;
		printf("%sCDC OBEX:\n"
		       "%s  bcdVersion           %x.%02x\n",
		       indent,
		       indent, buf[4], buf[3]);
		break;
	// case 0x16:		/* command set functional desc */
	// case 0x17:		/* command set detail desc */
	// case 0x18:		/* telephone control model functional desc */
	default:
		/* FIXME there are about a dozen more descriptor types */
		printf("%sUNRECOGNIZED: ", indent);
		dump_bytes(buf, buf[0]);
		return "unrecognized comm descriptor";
	}
	return 0;

  bad:
	printf("%sinvalid: ", indent);
	dump_bytes(buf, buf[0]);
	return "corrupt comm descriptor";
}

/* ---------------------------------------------------------------------- */

static void do_hub(struct usb_dev_handle *fd, unsigned has_tt)
{
	unsigned char buf [7];
	int i, ret;
	
	ret = usb_control_msg(fd,
			USB_ENDPOINT_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE,
			USB_REQ_GET_DESCRIPTOR,
			0x29 << 8, 0,
			buf, sizeof buf, CTRL_TIMEOUT);
	if (ret != sizeof buf) {
		/* Linux returns this for suspended devices */
		if (errno != EHOSTUNREACH)
			perror ("can't get hub descriptor");
		return;
	}
	dump_hub("", buf, has_tt);

	printf(" Hub Port Status:\n");
	for (i = 0; i < buf[2]; i++) {
		unsigned char stat [4];

		ret = usb_control_msg(fd,
				USB_ENDPOINT_IN | USB_TYPE_CLASS
					| USB_RECIP_OTHER,
				USB_REQ_GET_STATUS, 
				0, i + 1,
				stat, sizeof stat,
				CTRL_TIMEOUT);
		if (ret < 0) {
			fprintf(stderr,
				"cannot read port %d status, %s (%d)\n",
				i + 1, strerror(errno), errno);
			break;
		}

		printf("   Port %d: %02x%02x.%02x%02x", i + 1,
			stat[3], stat [2],
			stat[1], stat [0]);
		/* CAPS are used to highlight "transient" states */
		printf("%s%s%s%s%s",
			(stat[2] & 0x10) ? " C_RESET" : "",
			(stat[2] & 0x08) ? " C_OC" : "",
			(stat[2] & 0x04) ? " C_SUSPEND" : "",
			(stat[2] & 0x02) ? " C_ENABLE" : "",
			(stat[2] & 0x01) ? " C_CONNECT" : "");
		printf("%s%s%s%s%s%s%s%s%s%s\n",
			(stat[1] & 0x10) ? " indicator" : "",
			(stat[1] & 0x08) ? " test" : "",
			(stat[1] & 0x04) ? " highspeed" : "",
			(stat[1] & 0x02) ? " lowspeed" : "",
			(stat[1] & 0x01) ? " power" : "",
			(stat[0] & 0x10) ? " RESET" : "",
			(stat[0] & 0x08) ? " oc" : "",
			(stat[0] & 0x04) ? " suspend" : "",
			(stat[0] & 0x02) ? " enable" : "",
			(stat[0] & 0x01) ? " connect" : "");
	}
}

static void do_dualspeed(struct usb_dev_handle *fd)
{
	unsigned char buf [10];
	char cls[128], subcls[128], proto[128];
	int ret;
	
	ret = usb_control_msg(fd,
			USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE, 
			USB_REQ_GET_DESCRIPTOR,
			USB_DT_DEVICE_QUALIFIER << 8, 0,
			buf, sizeof buf, CTRL_TIMEOUT);
	if (ret < 0 && errno != EPIPE)
		perror ("can't get device qualifier");

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

static void do_debug(struct usb_dev_handle *fd)
{
	unsigned char buf [4];
	int ret;
	
	ret = usb_control_msg(fd,
			USB_ENDPOINT_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE, 
			USB_REQ_GET_DESCRIPTOR,
			USB_DT_DEBUG << 8, 0,
			buf, sizeof buf, CTRL_TIMEOUT);
	if (ret < 0 && errno != EPIPE)
		perror ("can't get debug descriptor");

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

static unsigned char *find_otg(unsigned char *buf, int buflen)
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

static void do_otg(struct usb_config_descriptor *config)
{
	unsigned	i, k;
	int		j;
	unsigned char	*desc;

	/* each config of an otg device has an OTG descriptor */
	desc = find_otg(config->extra, config->extralen);
	for (i = 0; !desc && i < config->bNumInterfaces; i++) {
		struct usb_interface *intf;

		intf = &config->interface [i];
		for (j = 0; !desc && j < intf->num_altsetting; j++) {
			struct usb_interface_descriptor *alt;

			alt = &intf->altsetting[j];
			desc = find_otg(alt->extra, alt->extralen);
			for (k = 0; !desc && k < alt->bNumEndpoints; k++) {
				struct usb_endpoint_descriptor *ep;

				ep = &alt->endpoint[k];
				desc = find_otg(ep->extra, ep->extralen);
			}
		}
	}
	if (!desc)
		return;

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
}

static void dumpdev(struct usb_device *dev)
{
	struct usb_dev_handle *udev;
	int i;

	udev = usb_open(dev);
	if (udev) {
		dump_device(udev, &dev->descriptor);
		if (dev->config) {
			for (i = 0; i < dev->descriptor.bNumConfigurations;
					i++) {
				dump_config(udev, &dev->config[i]);
			}
			do_otg(&dev->config[0]);
		}
		if (dev->descriptor.bDeviceClass == USB_CLASS_HUB)
			do_hub(udev, dev->descriptor.bDeviceProtocol);
		if (dev->descriptor.bcdUSB >= 0x0200) {
			do_dualspeed(udev);
			do_debug(udev);
		}
		usb_close(udev);
	}
	else
		fprintf(stderr, "Couldn't open device\n");
}

/* ---------------------------------------------------------------------- */

static int dump_one_device(const char *path)
{
	struct usb_device *dev;
	char vendor[128], product[128];

	dev = get_usb_device(path);
	if (!dev) {
		fprintf(stderr, "Cannot open %s\n", path);
		return 1;
	}
	get_vendor_string(vendor, sizeof(vendor), dev->descriptor.idVendor);
	get_product_string(product, sizeof(product), dev->descriptor.idVendor, dev->descriptor.idProduct);
	printf("Device: ID %04x:%04x %s %s\n", dev->descriptor.idVendor, 
					       dev->descriptor.idProduct, 
					       vendor, 
					       product);
	dumpdev(dev);
	return 0;
}

static int list_devices(int busnum, int devnum, int vendorid, int productid)
{
	struct usb_bus *bus;
	struct usb_device *dev;
	char vendor[128], product[128];
	int status;

	status=1; /* 1 device not found, 0 device found */
	
	for (bus = usb_busses; bus; bus = bus->next) {
		if (busnum != -1 && strtol(bus->dirname, NULL, 10) != busnum)
			continue;
		for (dev = bus->devices; dev; dev = dev->next) {
			if (devnum != -1 && strtol(dev->filename, NULL, 10)
					!= devnum)
				continue;
			if ((vendorid != -1 && vendorid
						!= dev->descriptor.idVendor)
					|| (productid != -1 && productid
						!= dev->descriptor.idProduct))
				continue;
			status = 0;
			get_vendor_string(vendor, sizeof(vendor),
					dev->descriptor.idVendor);
			get_product_string(product, sizeof(product),
					dev->descriptor.idVendor,
					dev->descriptor.idProduct);
			if (verblevel > 0)
				printf("\n");
			printf("Bus %s Device %s: ID %04x:%04x %s %s\n",
					bus->dirname, dev->filename,
					dev->descriptor.idVendor,
					dev->descriptor.idProduct,
					vendor, product);
			if (verblevel > 0)
				dumpdev(dev);
		}
	}
	return(status);
}

/* ---------------------------------------------------------------------- */

void devtree_busconnect(struct usbbusnode *bus)
{
	bus = bus;	/* reduce compiler warnings */
}

void devtree_busdisconnect(struct usbbusnode *bus)
{
	bus = bus;	/* reduce compiler warnings */
}

void devtree_devconnect(struct usbdevnode *dev)
{
	dev = dev;	/* reduce compiler warnings */
}

void devtree_devdisconnect(struct usbdevnode *dev)
{
	dev = dev;	/* reduce compiler warnings */
}

static int treedump(void)
{ 
	int fd;
	char buf[512];

	snprintf(buf, sizeof(buf), "%s/devices", procbususb);
	if ((fd = open(buf, O_RDONLY)) == -1) {
                fprintf(stderr, "cannot open %s, %s (%d)\n", buf, strerror(errno), errno);
                return(1);
	}
	devtree_parsedevfile(fd);
	close(fd);
        devtree_dump();
	return(0);
}

/* ---------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
        static const struct option long_options[] = {
                { "version", 0, 0, 'V' },
		{ "verbose", 0, 0, 'v' },
                { 0, 0, 0, 0 }
        };
	int c, err = 0;
	unsigned int allowctrlmsg = 0, treemode = 0;
	int bus = -1, devnum = -1, vendor = -1, product = -1;
	const char *devdump = NULL;
	char *cp;
	int status;

	while ((c = getopt_long(argc, argv, "D:vxtP:p:s:d:V",
			long_options, NULL)) != EOF) {
		switch(c) {
                case 'V':
                        printf("lsusb (" PACKAGE ") " VERSION "\n");
                        exit(0);
                        
		case 'v':
			verblevel++;
			break;

		case 'x':
			allowctrlmsg = 1;
			break;

		case 't':
			treemode = 1;
			break;

		case 's':
			cp = strchr(optarg, ':');
			if (cp) {
				*cp++ = 0;
				if (*optarg)
					bus = strtoul(optarg, NULL, 0);
				if (*cp)
					devnum = strtoul(cp, NULL, 0);
			} else {
				if (*optarg)
					devnum = strtoul(optarg, NULL, 0);
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
	if (err || argc > optind) {
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
			"  -t\n"
			"      Dump the physical USB device hierarchy as a tree\n"
			"  -V, --version\n"
			"      Show version of program\n"
			);
		exit(1);
	}

	/* by default, print names as well as numbers */
	if ((err = names_init("./usb.ids")) != 0) {
		if ((err = names_init(USBIDS_FILE)) != 0) {
			fprintf(stderr, "%s: cannot open \"%s\", %s\n",
					argv[0],
					USBIDS_FILE,
					strerror(err));
		}
	}
	status = 0;

	usb_init();

	usb_find_busses();
	usb_find_devices();
	      
	
	if (treemode) {
		/* treemode requires at least verblevel 1 */
 		verblevel += 1 - VERBLEVEL_DEFAULT;
		status = treedump();
	} else if (devdump)
		status = dump_one_device(devdump);
	else
		status = list_devices(bus, devnum, vendor, product);
	return status;
}

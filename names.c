/*****************************************************************************/
/*
 *      names.c  --  USB name database manipulation routines
 *
 *      Copyright (C) 1999, 2000  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *      Copyright (C) 2013  Tom Gundersen (teg@jklm.no)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include <libudev.h>

#ifdef HAVE_LIBZ
#include <zlib.h>
#define 	usb_file			gzFile
#define 	usb_fopen(path, mode) 		gzopen(path, mode)
#define 	usb_fgets(s, size, stream)	gzgets(stream, s, size)
#define 	usb_close(f)			gzclose(f)
#else
#define 	usb_file			FILE *
#define 	usb_fopen(path, mode)		fopen(path, mode)
#define 	usb_fgets(s, size, stream)	fgets(s, size, stream)
#define 	usb_close(f)			fclose(f)
#endif

#include "names.h"


/* ---------------------------------------------------------------------- */

struct audioterminal {
	struct audioterminal *next;
	u_int16_t termt;
	char name[1];
};

struct videoterminal {
	struct videoterminal *next;
	u_int16_t termt;
	char name[1];
};

struct genericstrtable {
	struct genericstrtable *next;
	unsigned int num;
	char name[1];
};

/* ---------------------------------------------------------------------- */

#define HASH1  0x10
#define HASH2  0x02
#define HASHSZ 16

static unsigned int hashnum(unsigned int num)
{
	unsigned int mask1 = HASH1 << 27, mask2 = HASH2 << 27;

	for (; mask1 >= HASH1; mask1 >>= 1, mask2 >>= 1)
		if (num & mask1)
			num ^= mask2;
	return num & (HASHSZ-1);
}

/* ---------------------------------------------------------------------- */

static struct udev *udev = NULL;
static struct udev_hwdb *hwdb = NULL;
static struct audioterminal *audioterminals[HASHSZ] = { NULL, };
static struct videoterminal *videoterminals[HASHSZ] = { NULL, };
static struct genericstrtable *hiddescriptors[HASHSZ] = { NULL, };
static struct genericstrtable *reports[HASHSZ] = { NULL, };
static struct genericstrtable *huts[HASHSZ] = { NULL, };
static struct genericstrtable *biass[HASHSZ] = { NULL, };
static struct genericstrtable *physdess[HASHSZ] = { NULL, };
static struct genericstrtable *hutus[HASHSZ] = { NULL, };
static struct genericstrtable *langids[HASHSZ] = { NULL, };
static struct genericstrtable *countrycodes[HASHSZ] = { NULL, };

/* ---------------------------------------------------------------------- */

static const char *names_genericstrtable(struct genericstrtable *t[HASHSZ],
					 unsigned int idx)
{
	struct genericstrtable *h;

	for (h = t[hashnum(idx)]; h; h = h->next)
		if (h->num == idx)
			return h->name;
	return NULL;
}

const char *names_hid(u_int8_t hidd)
{
	return names_genericstrtable(hiddescriptors, hidd);
}

const char *names_reporttag(u_int8_t rt)
{
	return names_genericstrtable(reports, rt);
}

const char *names_huts(unsigned int data)
{
	return names_genericstrtable(huts, data);
}

const char *names_hutus(unsigned int data)
{
	return names_genericstrtable(hutus, data);
}

const char *names_langid(u_int16_t langid)
{
	return names_genericstrtable(langids, langid);
}

const char *names_physdes(u_int8_t ph)
{
	return names_genericstrtable(physdess, ph);
}

const char *names_bias(u_int8_t b)
{
	return names_genericstrtable(biass, b);
}

const char *names_countrycode(unsigned int countrycode)
{
	return names_genericstrtable(countrycodes, countrycode);
}

static const char *hwdb_get(const char *modalias, const char *key)
{
	struct udev_list_entry *entry;

	udev_list_entry_foreach(entry, udev_hwdb_get_properties_list_entry(hwdb, modalias, 0))
		if (strcmp(udev_list_entry_get_name(entry), key) == 0)
			return udev_list_entry_get_value(entry);

	return NULL;
}

const char *names_vendor(u_int16_t vendorid)
{
	char modalias[64];

	sprintf(modalias, "usb:v%04X*", vendorid);
	return hwdb_get(modalias, "ID_VENDOR_FROM_DATABASE");
}

const char *names_product(u_int16_t vendorid, u_int16_t productid)
{
	char modalias[64];

	sprintf(modalias, "usb:v%04Xp%04X*", vendorid, productid);
	return hwdb_get(modalias, "ID_MODEL_FROM_DATABASE");
}

const char *names_class(u_int8_t classid)
{
	char modalias[64];

	sprintf(modalias, "usb:v*p*d*dc%02X*", classid);
	return hwdb_get(modalias, "ID_USB_CLASS_FROM_DATABASE");
}

const char *names_subclass(u_int8_t classid, u_int8_t subclassid)
{
	char modalias[64];

	sprintf(modalias, "usb:v*p*d*dc%02Xdsc%02X*", classid, subclassid);
	return hwdb_get(modalias, "ID_USB_SUBCLASS_FROM_DATABASE");
}

const char *names_protocol(u_int8_t classid, u_int8_t subclassid, u_int8_t protocolid)
{
	char modalias[64];

	sprintf(modalias, "usb:v*p*d*dc%02Xdsc%02Xdp%02X*", classid, subclassid, protocolid);
	return hwdb_get(modalias, "ID_USB_PROTOCOL_FROM_DATABASE");
}

const char *names_audioterminal(u_int16_t termt)
{
	struct audioterminal *at;

	at = audioterminals[hashnum(termt)];
	for (; at; at = at->next)
		if (at->termt == termt)
			return at->name;
	return NULL;
}

const char *names_videoterminal(u_int16_t termt)
{
	struct videoterminal *vt;

	vt = videoterminals[hashnum(termt)];
	for (; vt; vt = vt->next)
		if (vt->termt == termt)
			return vt->name;
	return NULL;
}

/* ---------------------------------------------------------------------- */

int get_vendor_string(char *buf, size_t size, u_int16_t vid)
{
        const char *cp;

        if (size < 1)
                return 0;
        *buf = 0;
        if (!(cp = names_vendor(vid)))
                return 0;
        return snprintf(buf, size, "%s", cp);
}

int get_product_string(char *buf, size_t size, u_int16_t vid, u_int16_t pid)
{
        const char *cp;

        if (size < 1)
                return 0;
        *buf = 0;
        if (!(cp = names_product(vid, pid)))
                return 0;
        return snprintf(buf, size, "%s", cp);
}

int get_class_string(char *buf, size_t size, u_int8_t cls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_class(cls)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

int get_subclass_string(char *buf, size_t size, u_int8_t cls, u_int8_t subcls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_subclass(cls, subcls)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

/* ---------------------------------------------------------------------- */

static int new_audioterminal(const char *name, u_int16_t termt)
{
	struct audioterminal *at;
	unsigned int h = hashnum(termt);

	at = audioterminals[h];
	for (; at; at = at->next)
		if (at->termt == termt)
			return -1;
	at = malloc(sizeof(struct audioterminal) + strlen(name));
	if (!at)
		return -1;
	strcpy(at->name, name);
	at->termt = termt;
	at->next = audioterminals[h];
	audioterminals[h] = at;
	return 0;
}

static int new_videoterminal(const char *name, u_int16_t termt)
{
	struct videoterminal *vt;
	unsigned int h = hashnum(termt);

	vt = videoterminals[h];
	for (; vt; vt = vt->next)
		if (vt->termt == termt)
			return -1;
	vt = malloc(sizeof(struct videoterminal) + strlen(name));
	if (!vt)
		return -1;
	strcpy(vt->name, name);
	vt->termt = termt;
	vt->next = videoterminals[h];
	videoterminals[h] = vt;
	return 0;
}

static int new_genericstrtable(struct genericstrtable *t[HASHSZ],
			       const char *name, unsigned int idx)
{
	struct genericstrtable *g;
	unsigned int h = hashnum(idx);

	for (g = t[h]; g; g = g->next)
		if (g->num == idx)
			return -1;
	g = malloc(sizeof(struct genericstrtable) + strlen(name));
	if (!g)
		return -1;
	strcpy(g->name, name);
	g->num = idx;
	g->next = t[h];
	t[h] = g;
	return 0;
}

static int new_hid(const char *name, u_int8_t hidd)
{
	return new_genericstrtable(hiddescriptors, name, hidd);
}

static int new_reporttag(const char *name, u_int8_t rt)
{
	return new_genericstrtable(reports, name, rt);
}

static int new_huts(const char *name, unsigned int data)
{
	return new_genericstrtable(huts, name, data);
}

static int new_hutus(const char *name, unsigned int data)
{
	return new_genericstrtable(hutus, name, data);
}

static int new_langid(const char *name, u_int16_t langid)
{
	return new_genericstrtable(langids, name, langid);
}

static int new_physdes(const char *name, u_int8_t ph)
{
	return new_genericstrtable(physdess, name, ph);
}
static int new_bias(const char *name, u_int8_t b)
{
	return new_genericstrtable(biass, name, b);
}

static int new_countrycode(const char *name, unsigned int countrycode)
{
	return new_genericstrtable(countrycodes, name, countrycode);
}

/* ---------------------------------------------------------------------- */

static void free_audioterminal(void)
{
	struct audioterminal *cur, *tmp;
	int i;

	for (i = 0; i < HASHSZ; i++) {
		cur = audioterminals[i];
		audioterminals[i] = NULL;
		while (cur) {
			tmp = cur;
			cur = cur->next;
			free(tmp);
		}
	}
	return;
}

static void free_videoterminal(void)
{
	struct videoterminal *cur, *tmp;
	int i;

	for (i = 0; i < HASHSZ; i++) {
		cur = videoterminals[i];
		videoterminals[i] = NULL;
		while (cur) {
			tmp = cur;
			cur = cur->next;
			free(tmp);
		}
	}
}

static void _free_genericstrtable(struct genericstrtable *t[HASHSZ])
{
	struct genericstrtable *cur, *tmp;
	int i;

	for (i = 0; i < HASHSZ; i++) {
		cur = t[i];
		t[i] = NULL;
		while (cur) {
			tmp = cur;
			cur = cur->next;
			free(tmp);
		}
	}
}

static void free_genericstrtable(void)
{
	_free_genericstrtable(hiddescriptors);
	_free_genericstrtable(reports);
	_free_genericstrtable(huts);
	_free_genericstrtable(biass);
	_free_genericstrtable(physdess);
	_free_genericstrtable(hutus);
	_free_genericstrtable(langids);
	_free_genericstrtable(countrycodes);
}

#define DBG(x)

static void parse(usb_file f)
{
	char buf[512], *cp;
	unsigned int linectr = 0;
	int lastvendor = -1;
	int lastclass = -1;
	int lastsubclass = -1;
	int lasthut = -1;
	int lastlang = -1;
	unsigned int u;

	while (usb_fgets(buf, sizeof(buf), f)) {
		linectr++;
		/* remove line ends */
		cp = strchr(buf, 13);
		if (cp)
			*cp = 0;
		cp = strchr(buf, 10);
		if (cp)
			*cp = 0;
		if (buf[0] == '#' || !buf[0])
			continue;
		cp = buf;
		if (buf[0] == 'P' && buf[1] == 'H' && buf[2] == 'Y' && buf[3] == 'S' && buf[4] == 'D' &&
		    buf[5] == 'E' && buf[6] == 'S' && /*isspace(buf[7])*/ buf[7] == ' ') {
			cp = buf + 8;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid Physdes type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid Physdes type at line %u\n", linectr);
				continue;
			}
			if (new_physdes(cp, u))
				fprintf(stderr, "Duplicate Physdes  type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u physdes type %02x %s\n", linectr, u, cp));
			continue;

		}
		if (buf[0] == 'P' && buf[1] == 'H' && buf[2] == 'Y' && /*isspace(buf[3])*/ buf[3] == ' ') {
			cp = buf + 4;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid PHY type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid PHY type at line %u\n", linectr);
				continue;
			}
			if (new_physdes(cp, u))
				fprintf(stderr, "Duplicate PHY type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u PHY type %02x %s\n", linectr, u, cp));
			continue;

		}
		if (buf[0] == 'B' && buf[1] == 'I' && buf[2] == 'A' && buf[3] == 'S' && /*isspace(buf[4])*/ buf[4] == ' ') {
			cp = buf + 5;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid BIAS type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid BIAS type at line %u\n", linectr);
				continue;
			}
			if (new_bias(cp, u))
				fprintf(stderr, "Duplicate BIAS  type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u BIAS type %02x %s\n", linectr, u, cp));
			continue;

		}
		if (buf[0] == 'L' && /*isspace(buf[1])*/ buf[1] == ' ') {
			cp =  buf+2;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid LANGID spec at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid LANGID spec at line %u\n", linectr);
				continue;
			}
			if (new_langid(cp, u))
				fprintf(stderr, "Duplicate LANGID spec at line %u language-id %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u LANGID %02x %s\n", linectr, u, cp));
			lasthut = lastclass = lastvendor = lastsubclass = -1;
			lastlang = u;
			continue;
		}
		if (buf[0] == 'A' && buf[1] == 'T' && isspace(buf[2])) {
			/* audio terminal type spec */
			cp = buf+3;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid audio terminal type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid audio terminal type at line %u\n", linectr);
				continue;
			}
			if (new_audioterminal(cp, u))
				fprintf(stderr, "Duplicate audio terminal type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u audio terminal type %02x %s\n", linectr, u, cp));
			continue;
		}
		if (buf[0] == 'V' && buf[1] == 'T' && isspace(buf[2])) {
			/* video terminal type spec */
			cp = buf+3;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid video terminal type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid video terminal type at line %u\n", linectr);
				continue;
			}
			if (new_videoterminal(cp, u))
				fprintf(stderr, "Duplicate video terminal type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u video terminal type %02x %s\n", linectr, u, cp));
			continue;
		}
		if (buf[0] == 'H' && buf[1] == 'C' && buf[2] == 'C' && isspace(buf[3])) {
			/* HID Descriptor bCountryCode */
			cp =  buf+3;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid HID country code at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 10);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid HID country code at line %u\n", linectr);
				continue;
			}
			if (new_countrycode(cp, u))
				fprintf(stderr, "Duplicate HID country code at line %u country %02u %s\n", linectr, u, cp);
			DBG(printf("line %5u keyboard country code %02u %s\n", linectr, u, cp));
			continue;
		}
		if (buf[0] == '\t' && isxdigit(buf[1])) {
			/* product or subclass spec */
			u = strtoul(buf+1, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid product/subclass spec at line %u\n", linectr);
				continue;
			}
			if (lasthut != -1) {
				if (new_hutus(cp, (lasthut << 16)+u))
					fprintf(stderr, "Duplicate HUT Usage Spec at line %u\n", linectr);
				continue;
			}
			if (lastlang != -1) {
				if (new_langid(cp, lastlang+(u<<10)))
					fprintf(stderr, "Duplicate LANGID Usage Spec at line %u\n", linectr);
				continue;
			}
			fprintf(stderr, "Product/Subclass spec without prior Vendor/Class spec at line %u\n", linectr);
			continue;
		}
		if (buf[0] == 'H' && buf[1] == 'I' && buf[2] == 'D' && /*isspace(buf[3])*/ buf[3] == ' ') {
			cp = buf + 4;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid HID type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid HID type at line %u\n", linectr);
				continue;
			}
			if (new_hid(cp, u))
				fprintf(stderr, "Duplicate HID type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u HID type %02x %s\n", linectr, u, cp));
			continue;

		}
		if (buf[0] == 'H' && buf[1] == 'U' && buf[2] == 'T' && /*isspace(buf[3])*/ buf[3] == ' ') {
			cp = buf + 4;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid HUT type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid HUT type at line %u\n", linectr);
				continue;
			}
			if (new_huts(cp, u))
				fprintf(stderr, "Duplicate HUT type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			lastlang = lastclass = lastvendor = lastsubclass = -1;
			lasthut = u;
			DBG(printf("line %5u HUT type %02x %s\n", linectr, u, cp));
			continue;

		}
		if (buf[0] == 'R' && buf[1] == ' ') {
			cp = buf + 2;
			while (isspace(*cp))
				cp++;
			if (!isxdigit(*cp)) {
				fprintf(stderr, "Invalid Report type at line %u\n", linectr);
				continue;
			}
			u = strtoul(cp, &cp, 16);
			while (isspace(*cp))
				cp++;
			if (!*cp) {
				fprintf(stderr, "Invalid Report type at line %u\n", linectr);
				continue;
			}
			if (new_reporttag(cp, u))
				fprintf(stderr, "Duplicate Report type spec at line %u terminal type %04x %s\n", linectr, u, cp);
			DBG(printf("line %5u Report type %02x %s\n", linectr, u, cp));
			continue;

		}
		fprintf(stderr, "Unknown line at line %u\n", linectr);
	}
}

/* ---------------------------------------------------------------------- */

int names_init(char *n)
{
	usb_file f;
	int r = 0;

	f = usb_fopen(n, "r");
	if (!f)
		r = errno;

	parse(f);
	usb_close(f);

	udev = udev_new();
	hwdb = udev_hwdb_new(udev);

	return r;
}

void names_exit(void)
{
	hwdb = udev_hwdb_unref(hwdb);
	udev = udev_unref(udev);
	free_audioterminal();
	free_videoterminal();
	free_genericstrtable();
}

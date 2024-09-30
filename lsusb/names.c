// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * USB name database manipulation routines
 *
 * Copyright (C) 1999, 2000 Thomas Sailer (sailer@ife.ee.ethz.ch)
 * Copyright (C) 2013 Tom Gundersen (teg@jklm.no)
 */
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

#include <libusb.h>
#include <libudev.h>

#include "usb-spec.h"
#include "names.h"
#include "sysfs.h"


#define HASH1  0x10
#define HASH2  0x02
#define HASHSZ 512

static unsigned int hashnum(unsigned int num)
{
	unsigned int mask1 = (unsigned int)HASH1 << 27, mask2 = (unsigned int)HASH2 << 27;

	for (; mask1 >= HASH1; mask1 >>= 1, mask2 >>= 1)
		if (num & mask1)
			num ^= mask2;
	return num & (HASHSZ-1);
}

/* ---------------------------------------------------------------------- */

static struct udev *udev = NULL;
static struct udev_hwdb *hwdb = NULL;
static struct audioterminal *audioterminals_hash[HASHSZ] = { NULL, };
static struct videoterminal *videoterminals_hash[HASHSZ] = { NULL, };
static struct genericstrtable *hiddescriptors_hash[HASHSZ] = { NULL, };
static struct genericstrtable *reports_hash[HASHSZ] = { NULL, };
static struct genericstrtable *huts_hash[HASHSZ] = { NULL, };
static struct genericstrtable *biass_hash[HASHSZ] = { NULL, };
static struct genericstrtable *physdess_hash[HASHSZ] = { NULL, };
static struct genericstrtable *hutus_hash[HASHSZ] = { NULL, };
static struct genericstrtable *langids_hash[HASHSZ] = { NULL, };
static struct genericstrtable *countrycodes_hash[HASHSZ] = { NULL, };

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

const char *names_hid(uint8_t hidd)
{
	return names_genericstrtable(hiddescriptors_hash, hidd);
}

const char *names_reporttag(uint8_t rt)
{
	return names_genericstrtable(reports_hash, rt);
}

const char *names_huts(unsigned int data)
{
	return names_genericstrtable(huts_hash, data);
}

const char *names_hutus(unsigned int data)
{
	return names_genericstrtable(hutus_hash, data);
}

const char *names_langid(uint16_t langid)
{
	return names_genericstrtable(langids_hash, langid);
}

const char *names_physdes(uint8_t ph)
{
	return names_genericstrtable(physdess_hash, ph);
}

const char *names_bias(uint8_t b)
{
	return names_genericstrtable(biass_hash, b);
}

const char *names_countrycode(unsigned int countrycode)
{
	return names_genericstrtable(countrycodes_hash, countrycode);
}

static const char *hwdb_get(const char *modalias, const char *key)
{
	struct udev_list_entry *entry;

	udev_list_entry_foreach(entry, udev_hwdb_get_properties_list_entry(hwdb, modalias, 0))
		if (strcmp(udev_list_entry_get_name(entry), key) == 0)
			return udev_list_entry_get_value(entry);

	return NULL;
}

const char *names_vendor(uint16_t vendorid)
{
	char modalias[64];

	sprintf(modalias, "usb:v%04X*", vendorid);
	return hwdb_get(modalias, "ID_VENDOR_FROM_DATABASE");
}

const char *names_product(uint16_t vendorid, uint16_t productid)
{
	char modalias[64];

	sprintf(modalias, "usb:v%04Xp%04X*", vendorid, productid);
	return hwdb_get(modalias, "ID_MODEL_FROM_DATABASE");
}

const char *names_class(uint8_t classid)
{
	char modalias[64];

	sprintf(modalias, "usb:v*p*d*dc%02X*", classid);
	return hwdb_get(modalias, "ID_USB_CLASS_FROM_DATABASE");
}

const char *names_subclass(uint8_t classid, uint8_t subclassid)
{
	char modalias[64];

	sprintf(modalias, "usb:v*p*d*dc%02Xdsc%02X*", classid, subclassid);
	return hwdb_get(modalias, "ID_USB_SUBCLASS_FROM_DATABASE");
}

const char *names_protocol(uint8_t classid, uint8_t subclassid, uint8_t protocolid)
{
	char modalias[64];

	sprintf(modalias, "usb:v*p*d*dc%02Xdsc%02Xdp%02X*", classid, subclassid, protocolid);
	return hwdb_get(modalias, "ID_USB_PROTOCOL_FROM_DATABASE");
}

const char *names_audioterminal(uint16_t termt)
{
	struct audioterminal *at;

	at = audioterminals_hash[hashnum(termt)];
	for (; at; at = at->next)
		if (at->termt == termt)
			return at->name;
	return NULL;
}

const char *names_videoterminal(uint16_t termt)
{
	struct videoterminal *vt;

	vt = videoterminals_hash[hashnum(termt)];
	for (; vt; vt = vt->next)
		if (vt->termt == termt)
			return vt->name;
	return NULL;
}

/* ---------------------------------------------------------------------- */

int get_vendor_string(char *buf, size_t size, uint16_t vid)
{
        const char *cp;

        if (size < 1)
                return 0;
        *buf = 0;
        if (!(cp = names_vendor(vid)))
		return 0;
        return snprintf(buf, size, "%s", cp);
}

int get_product_string(char *buf, size_t size, uint16_t vid, uint16_t pid)
{
        const char *cp;

        if (size < 1)
                return 0;
        *buf = 0;
        if (!(cp = names_product(vid, pid)))
		return 0;
        return snprintf(buf, size, "%s", cp);
}

int get_class_string(char *buf, size_t size, uint8_t cls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_class(cls)))
		return snprintf(buf, size, "[unknown]");
	return snprintf(buf, size, "%s", cp);
}

int get_subclass_string(char *buf, size_t size, uint8_t cls, uint8_t subcls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_subclass(cls, subcls)))
		return snprintf(buf, size, "[unknown]");
	return snprintf(buf, size, "%s", cp);
}

/*
 * Attempt to get friendly vendor and product names from the udev hwdb. If
 * either or both are not present, instead populate those from the device's
 * own string descriptors.
 */
void get_vendor_product_with_fallback(char *vendor, int vendor_len,
				      char *product, int product_len,
				      libusb_device *dev)
{
	struct libusb_device_descriptor desc;
	char sysfs_name[PATH_MAX];
	bool have_vendor, have_product;

	libusb_get_device_descriptor(dev, &desc);

	/* set to "[unknown]" by default unless something below finds a string */
	strncpy(vendor, "[unknown]", vendor_len);
	strncpy(product, "[unknown]", product_len);

	have_vendor = !!get_vendor_string(vendor, vendor_len, desc.idVendor);
	have_product = !!get_product_string(product, product_len,
			desc.idVendor, desc.idProduct);

	if (have_vendor && have_product)
		return;

	if (get_sysfs_name(sysfs_name, sizeof(sysfs_name), dev) >= 0) {
		if (!have_vendor)
			read_sysfs_prop(vendor, vendor_len, sysfs_name, "manufacturer");
		if (!have_product)
			read_sysfs_prop(product, product_len, sysfs_name, "product");
	}
}

/* ---------------------------------------------------------------------- */

static int hash_audioterminal(struct audioterminal *at)
{
	struct audioterminal *at_old;
	unsigned int h = hashnum(at->termt);

	for (at_old = audioterminals_hash[h]; at_old; at_old = at_old->next)
		if (at_old->termt == at->termt)
			return -1;
	at->next = audioterminals_hash[h];
	audioterminals_hash[h] = at;
	return 0;
}

static int hash_audioterminals(void)
{
	int r = 0, i, k;

	for (i = 0; audioterminals[i].name; i++)
	{
		k = hash_audioterminal(&audioterminals[i]);
		if (k < 0)
			r = k;
	}

	return r;
}

static int hash_videoterminal(struct videoterminal *vt)
{
	struct videoterminal *vt_old;
	unsigned int h = hashnum(vt->termt);

	for (vt_old = videoterminals_hash[h]; vt_old; vt_old = vt_old->next)
		if (vt_old->termt == vt->termt)
			return -1;
	vt->next = videoterminals_hash[h];
	videoterminals_hash[h] = vt;
	return 0;
}

static int hash_videoterminals(void)
{
	int r = 0, i, k;

	for (i = 0; videoterminals[i].name; i++)
	{
		k = hash_videoterminal(&videoterminals[i]);
		if (k < 0)
			r = k;
	}

	return r;
}

static int hash_genericstrtable(struct genericstrtable *t[HASHSZ],
			       struct genericstrtable *g)
{
	struct genericstrtable *g_old;
	unsigned int h = hashnum(g->num);

	for (g_old = t[h]; g_old; g_old = g_old->next)
		if (g_old->num == g->num)
			return -1;
	g->next = t[h];
	t[h] = g;
	return 0;
}

#define HASH_EACH(array, hash) \
	for (i = 0; array[i].name; i++) { \
		k = hash_genericstrtable(hash, &array[i]); \
		if (k < 0) { \
			r = k; \
		}\
	}

static int hash_tables(void)
{
	int r = 0, k, i;

	k = hash_audioterminals();
	if (k < 0)
		r = k;

	k = hash_videoterminals();
	if (k < 0)
		r = k;

	HASH_EACH(hiddescriptors, hiddescriptors_hash);

	HASH_EACH(reports, reports_hash);

	HASH_EACH(huts, huts_hash);

	HASH_EACH(hutus, hutus_hash);

	HASH_EACH(langids, langids_hash);

	HASH_EACH(physdess, physdess_hash);

	HASH_EACH(biass, biass_hash);

	HASH_EACH(countrycodes, countrycodes_hash);

	return r;
}

/* ---------------------------------------------------------------------- */

/*
static void print_tables(void)
{
	int i;
	struct audioterminal *at;
	struct videoterminal *vt;
	struct genericstrtable *li;
	struct genericstrtable *hu;


	printf("--------------------------------------------\n");
	printf("\t\t Audio Terminals\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		printf("hash: %d\n", i);
		at = audioterminals_hash[i];
		for (; at; at = at->next)
			printf("\tentry: %s\n", at->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Video Terminals\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		printf("hash: %d\n", i);
		vt = videoterminals_hash[i];
		for (; vt; vt = vt->next)
			printf("\tentry: %s\n", vt->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Languages\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		li = langids_hash[i];
		if (li)
			printf("hash: %d\n", i);
		for (; li; li = li->next)
			printf("\tid: %x, entry: %s\n", li->num, li->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Conutry Codes\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		hu = countrycodes_hash[i];
		if (hu)
			printf("hash: %d\n", i);
		for (; hu; hu = hu->next)
			printf("\tid: %x, entry: %s\n", hu->num, hu->name);
	}

	printf("--------------------------------------------\n");
}
*/

int names_init(void)
{
	int r;

	udev = udev_new();
	if (!udev)
		r = -1;
	else {
		hwdb = udev_hwdb_new(udev);
		if (!hwdb)
			r = -1;
	}

	r = hash_tables();

	return r;
}

void names_exit(void)
{
	hwdb = udev_hwdb_unref(hwdb);
	udev = udev_unref(udev);
}

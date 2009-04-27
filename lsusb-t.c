#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>

#include "list.h"

#define MY_SYSFS_FILENAME_LEN 255
#define MY_PATH_MAX 4096

struct usbinterface {
	struct list_head list;
	struct usbinterface *next;
	struct usbdevice *parent;
	unsigned int configuration;
	unsigned int ifnum;

	unsigned int bAlternateSetting;
	unsigned int bInterfaceClass;
	unsigned int bInterfaceNumber;
	unsigned int bInterfaceProtocol;
	unsigned int bInterfaceSubClass;
	unsigned int bNumEndpoints;

	char name[MY_SYSFS_FILENAME_LEN];
	char driver[MY_SYSFS_FILENAME_LEN];
};

struct usbdevice {
	struct list_head list;	/* connect devices independant of the bus */
	struct usbdevice *next;	/* next port on this hub */
	struct usbinterface *first_interface;	/* list of interfaces */
	struct usbdevice *first_child;	/* connect devices on this port */
	struct usbdevice *parent;	/* hub this device is connected to */
	unsigned int busnum;
	unsigned int parent_portnum;
	unsigned int portnum;

	unsigned int bConfigurationValue;
	unsigned int bDeviceClass;
	unsigned int bDeviceProtocol;
	unsigned int bDeviceSubClass;
	unsigned int bMaxPacketSize0;
	char bMaxPower[64];
	unsigned int bNumConfigurations;
	unsigned int bNumInterfaces;
	unsigned int bcdDevice;
	unsigned int bmAttributes;
	unsigned int configuration;
	unsigned int devnum;
	unsigned int idProduct;
	unsigned int idVendor;
	unsigned int maxchild;
	char manufacturer[64];
	char product[64];
	char serial[64];
	char version[64];
	char speed[4 + 1];	/* '1.5','12','480' + '\n' */

	char name[MY_SYSFS_FILENAME_LEN];
	char driver[MY_SYSFS_FILENAME_LEN];
};

struct usbbusnode {
	struct usbbusnode *next;
	struct usbinterface *first_interface;	/* list of interfaces */
	struct usbdevice *first_child;	/* connect childs belonging to this bus */
	unsigned int busnum;

	unsigned int bDeviceClass;
	unsigned int devnum;
	unsigned int maxchild;
	char speed[4 + 1];	/* '1.5','12','480' + '\n' */

	char driver[MY_SYSFS_FILENAME_LEN];
};

#define SYSFS_INTu(de,tgt, name) do { tgt->name = read_sysfs_file_int(de,#name,10); } while(0)
#define SYSFS_INTx(de,tgt, name) do { tgt->name = read_sysfs_file_int(de,#name,16); } while(0)
#define SYSFS_STR(de,tgt, name) do { read_sysfs_file_string(de, #name, tgt->name, MY_SYSFS_FILENAME_LEN); } while(0)

LIST_HEAD(interfacelist);
LIST_HEAD(usbdevlist);
static struct usbbusnode *usbbuslist;

static const char sys_bus_usb_devices[] = "/sys/bus/usb/devices";
static int indent;

#if 0
static void dump_usbbusnode(struct usbbusnode *b)
{
	printf(" B %p:'%u': n %p fi %p fc %p driver '%s'\n", b, b->busnum, b->next, b->first_interface, b->first_child, b->driver);
}

static void dump_usbdevice(struct usbdevice *d)
{
	printf
	    (" D %p:'%s': n %p fi %p fc %p bn %u ppn %u pn %u p %p bCV %u bDC %02x bDP %02x bDSC %02x bMPS %02x bMP '%s' bNC %u bNI %u bcdD %02x bmA %02x c %u dn %u idP %04x idV %04x mc %u m '%s' p '%s' s '%s' v '%s' sp '%s' driver '%s'\n",
	     d, d->name, d->next, d->first_interface, d->first_child, d->busnum, d->parent_portnum, d->portnum, d->parent, d->bConfigurationValue, d->bDeviceClass,
	     d->bDeviceProtocol, d->bDeviceSubClass, d->bMaxPacketSize0, d->bMaxPower, d->bNumConfigurations, d->bNumInterfaces, d->bcdDevice, d->bmAttributes,
	     d->configuration, d->devnum, d->idProduct, d->idVendor, d->maxchild, d->manufacturer, d->product, d->serial, d->version, d->speed, d->driver);
}

static void dump_usbinterface(struct usbinterface *i)
{
	printf(" I %p:'%s': n %p c %u if %u bAS %u bIC %02x bIN %02x bIP %02x bISC %02x bNE %u d '%s'\n", i, i->name, i->next, i->configuration, i->ifnum,
	       i->bAlternateSetting, i->bInterfaceClass, i->bInterfaceNumber, i->bInterfaceProtocol, i->bInterfaceSubClass, i->bNumEndpoints, i->driver);
}
#endif

static char tmp_str[128];
static const char *bInterfaceClass_to_str(unsigned int dc)
{
	const char *s;
	switch (dc) {
	case 0:
		s = ">ifc";
		break;
	case 1:
		s = "audio";
		break;
	case 2:
		s = "comm.";
		break;
	case 3:
		s = "HID";
		break;
	case 5:
		s = "PID";
		break;
	case 6:
		s = "still";
		break;
	case 7:
		s = "print";
		break;
	case 8:
		s = "stor.";
		break;
	case 9:
		s = "hub";
		break;
	case 10:
		s = "data";
		break;
	case 11:
		s = "scard";
		break;
	case 13:
		s = "c-sec";
		break;
	case 254:
		s = "app.";
		break;
	case 255:
		s = "vend.";
		break;
	default:
		snprintf(tmp_str, 128, "'bInterfaceClass 0x%02x not yet handled'", dc);;
		s = tmp_str;
	}
	return s;
}
static const char *bDeviceClass_to_str(unsigned int dc)
{
	const char *s;
	switch (dc) {
	case 9:
		s = "root_hub";
		break;
	default:
		snprintf(tmp_str, 128, "'bDeviceClass 0x%02x not yet handled'", dc);;
		s = tmp_str;
	}
	return s;
}
static void print_usbbusnode(struct usbbusnode *b)
{
	printf("/:  Bus %02u.Port %u: Dev %u, Class=%s, Driver=%s/%up, %sM\n", b->busnum, 1,
	       b->devnum, bDeviceClass_to_str(b->bDeviceClass), b->driver, b->maxchild, b->speed);
}

static void print_usbdevice(struct usbdevice *d, struct usbinterface *i)
{
	if (i->bInterfaceClass == 9)
		printf("Port %u: Dev %u, If %u, Class=%s, Driver=%s/%up, %sM\n", d->portnum, d->devnum, i->ifnum, bInterfaceClass_to_str(i->bInterfaceClass),
		       i->driver, d->maxchild, d->speed);
	else
		printf("Port %u: Dev %u, If %u, Class=%s, Driver=%s, %sM\n", d->portnum, d->devnum, i->ifnum, bInterfaceClass_to_str(i->bInterfaceClass), i->driver,
		       d->speed);
}

static unsigned int read_sysfs_file_int(const char *d_name, const char *file, int base)
{
	char buf[12], path[MY_PATH_MAX];
	int fd;
	ssize_t r;
	unsigned long ret;
	snprintf(path, MY_PATH_MAX, "%s/%s/%s", sys_bus_usb_devices, d_name, file);
	path[MY_PATH_MAX - 1] = '\0';
	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto error;
	memset(buf, 0, sizeof(buf));
	r = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (r < 0)
		goto error;
	buf[sizeof(buf) - 1] = '\0';
	ret = strtoul(buf, NULL, base);
	return (unsigned int)ret;

      error:
	perror(path);
	return 0;
}

static void read_sysfs_file_string(const char *d_name, const char *file, char *buf, int len)
{
	char path[MY_PATH_MAX];
	int fd;
	ssize_t r;
	fd = snprintf(path, MY_PATH_MAX, "%s/%s/%s", sys_bus_usb_devices, d_name, file);
	if (fd < 0 || fd >= MY_PATH_MAX)
		goto error;
	path[fd] = '\0';
	fd = open(path, O_RDONLY);
	if (fd < 0)
		goto error;
	r = read(fd, buf, len);
	close(fd);
	if (r >= 0 && r < len) {
		buf[r] = '\0';
		r--;
		while (buf[r] == '\n') {
			buf[r] = '\0';
			r--;
		}
		while (r) {
			if (buf[r] == '\n')
				buf[r] = ' ';
			r--;
		}
		return;
	}
      error:
	buf[0] = '\0';
}

static void append_dev_interface(struct usbinterface *i, struct usbinterface *new)
{
	while (i->next)
		i = i->next;
	i->next = new;
}

static void append_dev_sibling(struct usbdevice *d, struct usbdevice *new)
{
	while (d->next)
		d = d->next;
	d->next = new;
}

static void append_businterface(unsigned int busnum, struct usbinterface *new)
{
	struct usbbusnode *b = usbbuslist;
	struct usbinterface *i;
	while (b) {
		if (b->busnum == busnum) {
			i = b->first_interface;
			if (i) {
				while (i->next)
					i = i->next;
				i->next = new;
			} else
				b->first_interface = new;
			break;
		}
		b = b->next;
	}
}

static void append_busnode(struct usbbusnode *new)
{
	struct usbbusnode *b = usbbuslist;
	if (b) {
		while (b->next)
			b = b->next;
		b->next = new;
	} else
		usbbuslist = new;
}

static void add_usb_interface(const char *d_name)
{
	struct usbinterface *e;
	const char *p;
	char *pn, link[MY_PATH_MAX];
	unsigned long i;
	int l;
	p = strchr(d_name, ':');
	p++;
	i = strtoul(p, &pn, 10);
	if (!pn || p == pn)
		return;
	e = malloc(sizeof(struct usbinterface));
	if (!e)
		return;
	memset(e, 0, sizeof(struct usbinterface));
	e->configuration = i;
	p = pn + 1;
	i = strtoul(p, &pn, 10);
	if (!pn || p == pn)
		return;
	e->ifnum = i;
	if (snprintf(e->name, MY_SYSFS_FILENAME_LEN, "%s", d_name) >= MY_SYSFS_FILENAME_LEN)
		printf("warning: '%s' truncated to '%s'\n", e->name, d_name);
	SYSFS_INTu(d_name, e, bAlternateSetting);
	SYSFS_INTx(d_name, e, bInterfaceClass);
	SYSFS_INTx(d_name, e, bInterfaceNumber);
	SYSFS_INTx(d_name, e, bInterfaceProtocol);
	SYSFS_INTx(d_name, e, bInterfaceSubClass);
	SYSFS_INTx(d_name, e, bNumEndpoints);
	l = snprintf(link, MY_PATH_MAX, "%s/%s/driver", sys_bus_usb_devices, d_name);
	if (l > 0 && l < MY_PATH_MAX) {
		l = readlink(link, link, MY_PATH_MAX);
		if (l < 0)
			perror(d_name);
		else {
			if (l < MY_PATH_MAX - 1)
				link[l] = '\0';
			else
				link[0] = '\0';
			p = strrchr(link, '/');
			if (p)
				snprintf(e->driver, sizeof(e->driver), "%s", p + 1);
		}
	} else
		printf("Can not read driver link for '%s': %d\n", d_name, l);
	list_add_tail(&e->list, &interfacelist);
}

static void add_usb_device(const char *d_name)
{
	struct usbdevice *d;
	const char *p;
	char *pn, link[MY_PATH_MAX];
	unsigned long i;
	int l;
	p = d_name;
	i = strtoul(p, &pn, 10);
	if (!pn || p == pn)
		return;
	d = malloc(sizeof(struct usbdevice));
	if (!d)
		return;
	memset(d, 0, sizeof(struct usbdevice));
	d->busnum = i;
	while (pn) {
		p = pn + 1;
		i = strtoul(p, &pn, 10);
		if (p == pn)
			break;
		d->parent_portnum = d->portnum;
		d->portnum = i;
	}
	if (snprintf(d->name, MY_SYSFS_FILENAME_LEN, "%s", d_name) >= MY_SYSFS_FILENAME_LEN)
		printf("warning: '%s' truncated to '%s'\n", d->name, d_name);
	SYSFS_INTu(d_name, d, bConfigurationValue);
	SYSFS_INTx(d_name, d, bDeviceClass);
	SYSFS_INTx(d_name, d, bDeviceProtocol);
	SYSFS_INTx(d_name, d, bDeviceSubClass);
	SYSFS_INTx(d_name, d, bMaxPacketSize0);
	SYSFS_STR(d_name, d, bMaxPower);
	SYSFS_INTu(d_name, d, bNumConfigurations);
	SYSFS_INTx(d_name, d, bNumInterfaces);
	SYSFS_INTx(d_name, d, bcdDevice);
	SYSFS_INTx(d_name, d, bmAttributes);
	SYSFS_INTu(d_name, d, configuration);
	SYSFS_INTu(d_name, d, devnum);
	SYSFS_INTx(d_name, d, idProduct);
	SYSFS_INTx(d_name, d, idVendor);
	SYSFS_INTu(d_name, d, maxchild);
	SYSFS_STR(d_name, d, manufacturer);
	SYSFS_STR(d_name, d, product);
	SYSFS_STR(d_name, d, serial);
	SYSFS_STR(d_name, d, version);
	SYSFS_STR(d_name, d, speed);
	l = snprintf(link, MY_PATH_MAX, "%s/%s/driver", sys_bus_usb_devices, d_name);
	if (l > 0 && l < MY_PATH_MAX) {
		l = readlink(link, link, MY_PATH_MAX);
		if (l < 0)
			perror(d_name);
		else {
			if (l < MY_PATH_MAX - 1)
				link[l] = '\0';
			else
				link[0] = '\0';
			p = strrchr(link, '/');
			if (p)
				snprintf(d->driver, sizeof(d->driver), "%s", p + 1);
		}
	} else
		printf("Can not read driver link for '%s': %d\n", d_name, l);
	list_add_tail(&d->list, &usbdevlist);
}

static void get_roothub_driver(struct usbbusnode *b, const char *d_name)
{
	char *p, path[MY_PATH_MAX];
	int l;
	l = snprintf(path, MY_PATH_MAX, "%s/%s/../driver", sys_bus_usb_devices, d_name);
	if (l > 0 && l < MY_PATH_MAX) {
		l = readlink(path, path, MY_PATH_MAX);
		if (l < 0)
			perror(d_name);
		else {
			if (l < MY_PATH_MAX - 1)
				path[l] = '\0';
			else
				path[0] = '\0';
			p = strrchr(path, '/');
			if (p)
				snprintf(b->driver, sizeof(b->driver), "%s", p + 1);
		}
	} else
		printf("Can not read driver link for '%s': %d\n", d_name, l);
}

static void add_usb_bus(const char *d_name)
{
	struct usbbusnode *bus;
	bus = malloc(sizeof(struct usbbusnode));
	if (bus) {
		memset(bus, 0, sizeof(struct usbbusnode));
		bus->busnum = strtoul(d_name + 3, NULL, 10);
		SYSFS_INTu(d_name, bus, devnum);
		SYSFS_INTx(d_name, bus, bDeviceClass);
		SYSFS_INTu(d_name, bus, maxchild);
		SYSFS_STR(d_name, bus, speed);
		append_busnode(bus);
		get_roothub_driver(bus, d_name);
	}
}

static void inspect_bus_entry(const char *d_name)
{
	if (d_name[0] == '.' && (!d_name[1] || (d_name[1] == '.' && !d_name[2])))
		return;
	if (d_name[0] == 'u' && d_name[1] == 's' && d_name[2] == 'b' && isdigit(d_name[3])) {
		add_usb_bus(d_name);
	} else if (isdigit(d_name[0])) {
		if (strchr(d_name, ':'))
			add_usb_interface(d_name);
		else
			add_usb_device(d_name);
	} else
		fprintf(stderr, "ignoring '%s'\n", d_name);
}

static void walk_usb_devices(DIR * sbud)
{
	struct dirent *de;
	while ((de = readdir(sbud)))
		inspect_bus_entry(de->d_name);
}

static void assign_dev_to_bus(struct usbdevice *d)
{
	struct usbbusnode *b = usbbuslist;
	while (b) {
		if (b->busnum == d->busnum) {
			if (b->first_child)
				append_dev_sibling(b->first_child, d);
			else
				b->first_child = d;
		}
		b = b->next;
	}
}

static void assign_dev_to_parent(struct usbdevice *d)
{
	struct list_head *l;
	struct usbdevice *pd;
	char n[MY_SYSFS_FILENAME_LEN], *p;
	for (l = usbdevlist.next; l != &usbdevlist; l = l->next) {
		pd = list_entry(l, struct usbdevice, list);
		if (pd == d)
			continue;
		if (pd->busnum == d->busnum && pd->portnum == d->parent_portnum) {
			strcpy(n, d->name);
			p = strrchr(n, '.');
			if (p) {
				*p = '\0';
				if (strcmp(n, pd->name)) {
					continue;
				}
				d->parent = pd;
				if (pd->first_child)
					append_dev_sibling(pd->first_child, d);
				else
					pd->first_child = d;
				break;
			}
		}
	}
}

static void assign_interface_to_parent(struct usbdevice *d, struct usbinterface *i)
{
	const char *p;
	char *pn, name[MY_SYSFS_FILENAME_LEN];
	ptrdiff_t l;
	unsigned int busnum;

	p = strchr(i->name, ':');
	if (p) {
		l = p - i->name;
		if (l < MY_SYSFS_FILENAME_LEN) {
			memcpy(name, i->name, l);
			name[l] = '\0';
		} else
			name[0] = '\0';
		if (strcmp(d->name, name) == 0) {
			i->parent = d;
			if (d->first_interface)
				append_dev_interface(d->first_interface, i);
			else
				d->first_interface = i;
		} else {
			busnum = strtoul(name, &pn, 10);
			if (pn && pn != name) {
				if (p[1] == '0')
					append_businterface(busnum, i);
			}
		}
	}
}

static void connect_devices(void)
{
	struct list_head *ld, *li;
	struct usbdevice *d;
	struct usbinterface *e;
	for (ld = usbdevlist.next; ld != &usbdevlist; ld = ld->next) {
		d = list_entry(ld, struct usbdevice, list);
		if (d->parent_portnum)
			assign_dev_to_parent(d);
		else
			assign_dev_to_bus(d);
		for (li = interfacelist.next; li != &interfacelist; li = li->next) {
			e = list_entry(li, struct usbinterface, list);
			if (!e->parent)
				assign_interface_to_parent(d, e);
		}
	}
	for (li = interfacelist.next; li != &interfacelist; li = li->next) {
		e = list_entry(li, struct usbinterface, list);
	}
}

static void sort_dev_interfaces(struct usbinterface **i)
{
	struct usbinterface *t, *p, **pp;
	int swapped;
	p = *i;
	pp = i;
	do {
		p = *i;
		pp = i;
		swapped = 0;
		while (p->next) {
			if (p->configuration > p->next->configuration) {
				t = p->next;
				p->next = t->next;
				t->next = p;
				*pp = t;
				swapped = 1;
				p = t;
			}
			if (p->ifnum > p->next->ifnum) {
				t = p->next;
				p->next = t->next;
				t->next = p;
				*pp = t;
				swapped = 1;
				p = t;
			}
			pp = &p->next;
			p = p->next;
		}
	} while (swapped);
}

static void sort_dev_siblings(struct usbdevice **d)
{
	struct usbdevice *t, *p, **pp;
	int swapped;
	p = *d;
	pp = d;
	if (p->first_child)
		sort_dev_siblings(&p->first_child);
	if (p->first_interface)
		sort_dev_interfaces(&p->first_interface);
	do {
		p = *d;
		pp = d;
		swapped = 0;
		while (p->next) {
			if (p->portnum > p->next->portnum) {
				t = p->next;
				p->next = t->next;
				t->next = p;
				*pp = t;
				swapped = 1;
				p = t;
			}
			pp = &p->next;
			p = p->next;
		}
	} while (swapped);
}

static void sort_devices(void)
{
	struct usbbusnode *b = usbbuslist;
	while (b) {
		if (b->first_child)
			sort_dev_siblings(&b->first_child);
		b = b->next;
	}
}

static void sort_busses(void)
{
	/* need to reverse sort bus numbers */
	struct usbbusnode *t, *p, **pp;
	int swapped;
	do {
		p = usbbuslist;
		pp = &usbbuslist;
		swapped = 0;
		while (p->next) {
			if (p->busnum < p->next->busnum) {
				t = p->next;
				p->next = t->next;
				t->next = p;
				*pp = t;
				swapped = 1;
				p = t;
			}
			pp = &p->next;
			p = p->next;
		}
	} while (swapped);
}

static void print_tree_dev_interface(struct usbdevice *d, struct usbinterface *i)
{
	indent += 3;
	while (i) {
		printf(" %*s", indent, "|__ ");
		print_usbdevice(d, i);
		i = i->next;
	}
	indent -= 3;
}
static void print_tree_dev_children(struct usbdevice *d)
{
	indent += 4;
	while (d) {
		print_tree_dev_interface(d, d->first_interface);
		print_tree_dev_children(d->first_child);
		d = d->next;
	}
	indent -= 4;
}

static void print_tree(void)
{
	struct usbbusnode *b = usbbuslist;
	while (b) {
		print_usbbusnode(b);
		if (b->first_child)
			print_tree_dev_children(b->first_child);
		b = b->next;
	}
}

int lsusb_t(void)
{
	DIR *sbud = opendir(sys_bus_usb_devices);
	if (sbud) {
		walk_usb_devices(sbud);
		closedir(sbud);
		connect_devices();
		sort_devices();
		sort_busses();
		print_tree();
	} else
		perror(sys_bus_usb_devices);
	return sbud == NULL;
}

/* Declaring the usb_device_id fields as unsigned int simplifies
   the sscanf call. */

#define USB_MATCH_VENDOR		0x0001
#define USB_MATCH_PRODUCT		0x0002
#define USB_MATCH_DEV_LO		0x0004
#define USB_MATCH_DEV_HI		0x0008
#define USB_MATCH_DEV_CLASS		0x0010
#define USB_MATCH_DEV_SUBCLASS		0x0020
#define USB_MATCH_DEV_PROTOCOL		0x0040
#define USB_MATCH_INT_CLASS		0x0080
#define USB_MATCH_INT_SUBCLASS		0x0100
#define USB_MATCH_INT_PROTOCOL		0x0200

struct usbmap_entry {
	unsigned int		match_flags;
	/*
	 * vendor/product codes are checked, if vendor is nonzero
	 * Range is for device revision (bcdDevice), inclusive;
	 * zero values here mean range isn't considered
	 */
	unsigned int		idVendor;
	unsigned int		idProduct;
	unsigned int		bcdDevice_lo, bcdDevice_hi;

	/*
	 * if device class != 0, these can be match criteria;
	 * but only if this bDeviceClass value is nonzero
	 */
	unsigned int		bDeviceClass;
	unsigned int		bDeviceSubClass;
	unsigned int		bDeviceProtocol;

	/*
	 * if interface class != 0, these can be match criteria;
	 * but only if this bUnsigned LongerfaceClass value is nonzero
	 */
	unsigned int		bInterfaceClass;
	unsigned int		bInterfaceSubClass;
	unsigned int		bInterfaceProtocol;

	/*
	 * for driver's use; not involved in driver matching.
	 */
	unsigned long	driver_info;

	char *name;
	int selected;
	int *selected_ptr;
	struct usbmap_entry *next;
};

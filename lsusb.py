#!/usr/bin/env python
# lsusb.py
# Displays your USB devices in reasonable form.
# (c) Kurt Garloff <garloff@suse.de>, 2/2009, GPL v2 or v3.
# Usage: See usage()

import os, sys, re, getopt

# from __future__ import print_function

# Global options
showint = False
showhubint = False
noemptyhub = False
nohub = False
warnsort = False

prefix = "/sys/bus/usb/devices/"
usbids = "/usr/share/usb.ids"

esc = chr(27)
norm = esc + "[0;0m"
bold = esc + "[0;1m"
red =  esc + "[0;31m"
green= esc + "[0;32m"
amber= esc + "[0;33m"

cols = ("", "", "", "", "")

def readattr(path, name):
	"Read attribute from sysfs and return as string"
	f = open(prefix + path + "/" + name);
	return f.readline().rstrip("\n");

def readlink(path, name):
	"Read symlink and return basename"
	return os.path.basename(os.readlink(prefix + path + "/" + name));

class UsbClass:
	"Container for USB Class/Subclass/Protocol"
	def __init__(self, cl, sc, pr, str = ""):
		self.pclass = cl
		self.subclass = sc
		self.proto = pr
		self.desc = str
	def __repr__(self):
		return self.desc
	def __cmp__(self, oth):
		# Works only on 64bit systems:
		#return self.pclass*0x10000+self.subclass*0x100+self.proto \
		#	- oth.pclass*0x10000-oth.subclass*0x100-oth.proto
		if self.pclass != oth.pclass:
			return self.pclass - oth.pclass
		if self.subclass != oth.subclass:
			return self.subclass - oth.subclass
		return self.proto - oth.proto

class UsbVendor:
	"Container for USB Vendors"
	def __init__(self, vid, vname = ""):
		self.vid = vid
		self.vname = vname
	def __repr__(self):
		return self.vname
	def __cmp__(self, oth):
		return self.vid - oth.vid

class UsbProduct:
	"Container for USB VID:PID devices"
	def __init__(self, vid, pid, pname = ""):
		self.vid = vid
		self.pid = pid
		self.pname = pname
	def __repr__(self):
		return self.pname
	def __cmp__(self, oth):
		# Works only on 64bit systems:
		# return self.vid*0x10000 + self.pid \
		#	- oth.vid*0x10000 - oth.pid
		if self.vid != oth.vid:
			return self.vid - oth.vid
		return self.pid - oth.pid

usbvendors = []
usbproducts = []
usbclasses = []

def ishexdigit(str):
	"return True if all digits are valid hex digits"
	for dg in str:
		if not dg.isdigit() and not dg in 'abcdef':
			return False
	return True

def parse_usb_ids():
	"Parse /usr/share/usb.ids and fill usbvendors, usbproducts, usbclasses"
	id = 0
	sid = 0
	mode = 0
	strg = ""
	cstrg = ""
	for ln in file(usbids, "r").readlines():
		if ln[0] == '#':
			continue
		ln = ln.rstrip('\n')
		if len(ln) == 0:
			continue
		if ishexdigit(ln[0:4]):
			mode = 0
			id = int(ln[:4], 16)
			usbvendors.append(UsbVendor(id, ln[6:]))
			continue
		if ln[0] == '\t' and ishexdigit(ln[1:3]):
			sid = int(ln[1:5], 16)
			# USB devices
			if mode == 0:
				usbproducts.append(UsbProduct(id, sid, ln[7:]))
				continue
			elif mode == 1:
				nm = ln[5:]
				if nm != "Unused":
					strg = cstrg + ":" + nm
				else:
					strg = cstrg + ":"
				usbclasses.append(UsbClass(id, sid, -1, strg))
				continue
		if ln[0] == 'C':
			mode = 1
			id = int(ln[2:4], 16)
			cstrg = ln[6:]
			usbclasses.append(UsbClass(id, -1, -1, cstrg))
			continue
		if mode == 1 and ln[0] == '\t' and ln[1] == '\t' and ishexdigit(ln[2:4]):
			prid = int(ln[2:4], 16)
			usbclasses.append(UsbClass(id, sid, prid, strg + ":" + ln[6:]))
			continue
		mode = 2

def bin_search(first, last, item, list):
	"binary search on list, returns -1 on fail, match idx otherwise, recursive"
	#print "bin_search(%i,%i)" % (first, last)
	if first == last:
		return -1
	if first == last-1:
		if item == list[first]:
			return first
		else:
			return -1
	mid = (first+last) // 2
	if item == list[mid]:
		return mid
	elif item < list[mid]:
		return bin_search(first, mid, item, list)
	else:
		return bin_search(mid, last, item, list)


def find_usb_prod(vid, pid):
	"Return device name from USB Vendor:Product list"
	strg = ""
	dev = UsbVendor(vid, "")
	lnvend = len(usbvendors)
	ix = bin_search(0, lnvend, dev, usbvendors)
	if ix != -1:
		strg = usbvendors[ix].__repr__()
	else:
		return ""
	dev = UsbProduct(vid, pid, "")
	lnprod = len(usbproducts)
	ix = bin_search(0, lnprod, dev, usbproducts)
	if ix != -1:
		return strg + " " + usbproducts[ix].__repr__()
	return strg

def find_usb_class(cid, sid, pid):
	"Return USB protocol from usbclasses list"
	if cid == 0xff and sid == 0xff and pid == 0xff:
		return "Vendor Specific"
	lnlst = len(usbclasses)
	dev = UsbClass(cid, sid, pid, "")
	ix = bin_search(0, lnlst, dev, usbclasses)
	if ix != -1:
		return usbclasses[ix].__repr__()
	dev = UsbClass(cid, sid, -1, "")
	ix = bin_search(0, lnlst, dev, usbclasses)
	if ix != -1:
		return usbclasses[ix].__repr__()
	dev = UsbClass(cid, -1, -1, "")
	ix = bin_search(0, lnlst, dev, usbclasses)
	if ix != -1:
		return usbclasses[ix].__repr__()
	return ""


devlst = (	'host', 	# usb-storage
		'video4linux/video', 	# uvcvideo et al.
		'sound/card',	# snd-usb-audio 
		'net/', 	# cdc_ether, ...
		'input/input',	# usbhid
		'usb:hiddev',	# usb hid
		'bluetooth/hci',	# btusb
		'ttyUSB',	# btusb
		'tty/',		# cdc_acm
		'usb:lp',	# usblp
		#'usb/lp',	# usblp 
		'usb/',		# hiddev, usblp
	)

def find_storage(hostno):
	"Return SCSI block dev names for host"
	res = ""
	for ent in os.listdir("/sys/class/scsi_device/"):
		(host, bus, tgt, lun) = ent.split(":")
		if host == hostno:
			try:
				for ent2 in os.listdir("/sys/class/scsi_device/%s/device/block" % ent):
					res += ent2 + " "
			except:
				pass
	return res

def find_dev(driver, usbname):
	"Return pseudo devname that's driven by driver"
	res = ""
	for nm in devlst:
		dir = prefix + usbname
		prep = ""
		#print nm
		idx = nm.find('/')
		if idx != -1:
			prep = nm[:idx+1]
			dir += "/" + nm[:idx]
			nm = nm[idx+1:]
		ln = len(nm)
		try:
			for ent in os.listdir(dir):
				if ent[:ln] == nm:
					res += prep+ent+" "
					if nm == "host":
						res += "(" + find_storage(ent[ln:])[:-1] + ")"
		except:
			pass
	return res


class UsbInterface:
	"Container for USB interface info"
	def __init__(self, parent = None, level = 1):
		self.parent = parent
		self.level = level
		self.fname = ""
		self.iclass = 0
		self.isclass = 0
		self.iproto = 0
		self.noep = 0
		self.driver = ""
		self.devname = ""
		self.protoname = ""
	def read(self, fname):
		fullpath = ""
		if self.parent:
			fullpath += self.parent.fname + "/"
		fullpath += fname
		#self.fname = fullpath
		self.fname = fname
		self.iclass = int(readattr(fullpath, "bInterfaceClass"),16)
		self.isclass = int(readattr(fullpath, "bInterfaceSubClass"),16)
		self.iproto = int(readattr(fullpath, "bInterfaceProtocol"),16)
		self.noep = int(readattr(fullpath, "bNumEndpoints"))
		try:
			self.driver = readlink(fname, "driver")
			self.devname = find_dev(self.driver, fname)
		except:
			pass
		self.protoname = find_usb_class(self.iclass, self.isclass, self.iproto)
	def __str__(self):
		return "%-16s(IF) %02x:%02x:%02x %iEPs (%s) %s%s %s%s%s\n" % \
			(" " * self.level+self.fname, self.iclass,
			 self.isclass, self.iproto, self.noep,
			 self.protoname, 
			 cols[3], self.driver,
			 cols[4], self.devname, cols[0])

class UsbDevice:
	"Container for USB device info"
	def __init__(self, parent = None, level = 0):
		self.parent = parent
		self.level = level
		self.fname = ""
		self.iclass = 0
		self.isclass = 0
		self.iproto = 0
		self.vid = 0
		self.pid = 0
		self.name = ""
		self.usbver = ""
		self.speed = ""
		self.maxpower = ""
		self.noports = 0
		self.nointerfaces = 0
		self.driver = ""
		self.devname = ""
		self.interfaces = []
		self.children = []

	def read(self, fname):
		self.fname = fname
		self.iclass = int(readattr(fname, "bDeviceClass"), 16)
		self.isclass = int(readattr(fname, "bDeviceSubClass"), 16)
		self.iproto = int(readattr(fname, "bDeviceProtocol"), 16)
		self.vid = int(readattr(fname, "idVendor"), 16)
		self.pid = int(readattr(fname, "idProduct"), 16)
		try:
			self.name = readattr(fname, "manufacturer") + " " \
				  + readattr(fname, "product")
			#self.name += " " + readattr(fname, "serial")
			if self.name[:5] == "Linux":
				rx = re.compile(r"Linux [^ ]* (.hci_hcd) .HCI Host Controller")
				mch = rx.match(self.name)
				if mch:
					self.name = mch.group(1)

		except:
			pass
		if not self.name:
			self.name = find_usb_prod(self.vid, self.pid)
		# Some USB Card readers have a better name then Generic ...
		if self.name[:7] == "Generic":
			oldnm = self.name
			self.name = find_usb_prod(self.vid, self.pid)
			if not self.name:
				self.name = oldnm
		try:
			ser = readattr(fname, "serial")
			# Some USB devs report "serial" as serial no. suppress
			if (ser and ser != "serial"):
				self.name += " " + ser
		except:
			pass
		self.usbver = readattr(fname, "version")
		self.speed = readattr(fname, "speed")
		self.maxpower = readattr(fname, "bMaxPower")
		self.noports = int(readattr(fname, "maxchild"))
		try:
			self.nointerfaces = int(readattr(fname, "bNumInterfaces"))
		except:
			#print "ERROR: %s/bNumInterfaces = %s" % (fname,
			#		readattr(fname, "bNumInterfaces"))a
			self.nointerfaces = 0
		try:
			self.driver = readlink(fname, "driver")
			self.devname = find_dev(self.driver, fname)
		except:
			pass

	def readchildren(self):
		if self.fname[0:3] == "usb":
			fname = self.fname[3:]
		else:
			fname = self.fname
		for dirent in os.listdir(prefix + self.fname):
			if not dirent[0:1].isdigit():
				continue
			#print dirent
			if os.access(prefix + dirent + "/bInterfaceClass", os.R_OK):
				iface = UsbInterface(self, self.level+1)
				iface.read(dirent)
				self.interfaces.append(iface)
			else:
				usbdev = UsbDevice(self, self.level+1)
				usbdev.read(dirent)
				usbdev.readchildren()
				self.children.append(usbdev)

	def __str__(self):
		#str = " " * self.level + self.fname
		if self.iclass == 9:
			col = cols[2]
			if noemptyhub and len(self.children) == 0:
				return ""
			if nohub:
				str = ""
		else:
			col = cols[1]
		if not nohub or self.iclass != 9:
			str = "%-16s%s%04x:%04x%s %02x %s%5sMBit/s %s %iIFs (%s%s%s)" % \
				(" " * self.level + self.fname, 
				 cols[1], self.vid, self.pid, cols[0],
				 self.iclass, self.usbver, self.speed, self.maxpower,
				 self.nointerfaces, col, self.name, cols[0])
			#if self.driver != "usb":
			#	str += " %s" % self.driver
			if self.iclass == 9 and not showhubint:
				str += " %shub%s\n" % (cols[2], cols[0])
			else:
				str += "\n"
				if showint:	
					for iface in self.interfaces:
						str += iface.__str__()
		for child in self.children:
			str += child.__str__()
		return str

def deepcopy(lst):
	"Returns a deep copy from the list lst"
	copy = []
	for item in lst:
		copy.append(item)
	return copy

def display_diff(lst1, lst2, fmtstr, args):
	"Compare lists (same length!) and display differences"
	for idx in range(0, len(lst1)):
		if lst1[idx] != lst2[idx]:
			print "Warning: " + fmtstr % args(lst2[idx])

def fix_usbvend():
	"Sort USB vendor list and (optionally) display diffs"
	if warnsort:
		oldusbvend = deepcopy(usbvendors)
	usbvendors.sort()
	if warnsort:
		display_diff(usbvendors, oldusbvend, 
				"Unsorted Vendor ID %04x",
				lambda x: (x.vid,))

def fix_usbprod():
	"Sort USB products list"
	if warnsort:
		oldusbprod = deepcopy(usbproducts)
	usbproducts.sort()
	if warnsort:
		display_diff(usbproducts, oldusbprod, 
				"Unsorted Vendor:Product ID %04x:%04x",
				lambda x: (x.vid, x.pid))

def fix_usbclass():
	"Sort USB class list"
	if warnsort:
		oldusbcls = deepcopy(usbclasses)
	usbclasses.sort()
	if warnsort:
		display_diff(usbclasses, oldusbcls,
				"Unsorted USB class %02x:%02x:%02x",
				lambda x: (x.pclass, x.subclass, x.proto))


def usage():
	"Displays usage information"
	print "Usage: lsusb.py [options]"
	print "Options:"
	print " -h display this help"
	print " -i display interface information"
	print " -I display interface information, even for hubs"
	print " -u suppress empty hubs"
	print " -U suppress all hubs"
	print " -c use colors"
	print " -w display warning if usb.ids is not sorted correctly"
	print " -f FILE override filename for /usr/share/usb.ids"
	return 2

def read_usb():
	"Read toplevel USB entries and print"
	for dirent in os.listdir(prefix):
		#print dirent,
		if not dirent[0:3] == "usb":
			continue
		usbdev = UsbDevice(None, 0)
		usbdev.read(dirent)
		usbdev.readchildren()
		os.write(sys.stdout.fileno(), usbdev.__str__())

def main(argv):
	"main entry point"
	global showint, showhubint, noemptyhub, nohub, warnsort, cols, usbids
	try:
		(optlist, args) = getopt.gnu_getopt(argv[1:], "hiIuUwcf:", ("help",))
	except getopt.GetoptError, exc:
		print "Error:", exc
		sys.exit(usage())
	for opt in optlist:
		if opt[0] == "-h" or opt[0] == "--help":
			usage()
			sys.exit(0)
		if opt[0] == "-i":
			showint = True
			continue
		if opt[0] == "-I":
			showint = True
			showhubint = True
			continue
		if opt[0] == "-u":
			noemptyhub = True
			continue
		if opt[0] == "-U":
			noemptyhub = True
			nohub = True
			continue
		if opt[0] == "-c":
			cols = (norm, bold, red, green, amber)
			continue
		if opt[0] == "-w":
			warnsort = True
			continue
		if opt[0] == "-f":
			usbids = opt[1]
			continue
	if len(args) > 0:
		print "Error: excess args %s ..." % args[0]
		sys.exit(usage())

	try:
		parse_usb_ids()
		fix_usbvend()	
		fix_usbprod()
		fix_usbclass()
	except:
		print >>sys.stderr, " WARNING: Failure to read usb.ids"
		print >>sys.stderr, sys.exc_info()
	read_usb()

# Entry point
if __name__ == "__main__":
	main(sys.argv)



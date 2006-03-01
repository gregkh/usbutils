%define name    usbutils
%define version 0.72
%define release 1
Name:		%{name}
Version:	%{version}
Release:        %{release}
Source:		http://prdownloads.sourceforge.net/linux-usb/${name}-${version}.tar.gz
Copyright:	GNU GPL
Buildroot: 	/tmp/%{name}-%{version}-root
ExclusiveOS: 	Linux
Summary: Linux USB utilities.
Group: Applications/System
BuildPrereq: libusb-devel

%description
This package contains "lsusb", a utility for inspecting
devices connected to systems serving as USB hosts.

It requires a Linux kernel with support for /proc/bus/usb
interface.  

%prep
%setup -q

%build
CFLAGS="$RPM_OPT_FLAGS -Wall" ./configure --prefix=/usr --datadir=/usr/share --sbindir=/sbin
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
#install -d $RPM_BUILD_ROOT/{sbin,/usr/man/man8,/usr/share}

#install -s lsusb $RPM_BUILD_ROOT/sbin
#install lsusb.8 $RPM_BUILD_ROOT/usr/man/man8
#install usb.ids $RPM_BUILD_ROOT/usr/share

%files
%defattr(0644, root, root, 0755)
%attr(0644, root, man) /usr/man/man8/lsusb.8.*
%attr(0711, root, root) /sbin/lsusb
%config /usr/share/usb.ids
%doc AUTHORS COPYING ChangeLog INSTALL NEWS README

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
* Mon Jun 17 2002 Thomas Sailer <t.sailer@alumni.ethz.ch>
- rev 0.10
- Patch from Ambrose Li <a.c.li@ieee.org>

* Fri Nov 23 2001 Thomas Sailer <t.sailer@alumni.ethz.ch>
- rev 0.9
- no longer package usbmodules, as it conflicts with hotplug from RedHat 7.2

* Tue Jun 12 2001 Thomas Sailer <t.sailer@alumni.ethz.ch>
- rev 0.8

* Tue Sep 14 1999 Thomas Sailer <sailer@ife.ee.ethz.ch>
- Initial build

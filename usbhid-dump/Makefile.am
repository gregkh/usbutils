# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (C) 2009-2010 Nikolai Kondrashov
#
# This file is part of usbhid-dump.

SUBDIRS = src

dist_noinst_SCRIPTS = bootstrap

ACLOCAL_AMFLAGS = -I m4

dist-hook:
	@set -e; \
	if test -d "$(srcdir)/.git"; then \
		echo Generating ChangeLog; \
		( cd "$(srcdir)" && auxdir/missing --run git log ) > \
			ChangeLog.tmp; \
		mv -f ChangeLog.tmp "$(distdir)/ChangeLog"; \
	else \
		echo Skipping ChangeLog generation: no .git directory >&2; \
	fi


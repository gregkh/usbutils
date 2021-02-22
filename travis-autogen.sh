#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright 2016 Stephan Linz <linz@li-pro.net>

# -Wsign-compare -Wtype-limits
# Warnings enabled
CFLAGS="-Wall -Wextra"

CFLAGS+=" -Wbad-function-cast"
#CFLAGS+=" -Wcast-align"
CFLAGS+=" -Wchar-subscripts"
CFLAGS+=" -Wempty-body"
CFLAGS+=" -Wformat=2"
#CFLAGS+=" -Wformat"
#CFLAGS+=" -Wformat-nonliteral"
#CFLAGS+=" -Wformat-security"
#CFLAGS+=" -Wformat-y2k"
CFLAGS+=" -Winit-self"
CFLAGS+=" -Winline"
CFLAGS+=" -Wmissing-declarations"
CFLAGS+=" -Wmissing-include-dirs"
CFLAGS+=" -Wmissing-prototypes"
CFLAGS+=" -Wnested-externs"
CFLAGS+=" -Wold-style-definition"
CFLAGS+=" -Wpointer-arith"
CFLAGS+=" -Wredundant-decls"
CFLAGS+=" -Wshadow"
CFLAGS+=" -Wstrict-prototypes"
CFLAGS+=" -Wswitch-enum"
CFLAGS+=" -Wundef"
CFLAGS+=" -Wuninitialized"
CFLAGS+=" -Wunused"
CFLAGS+=" -Wunused-variable"
CFLAGS+=" -Wsign-compare"
CFLAGS+=" -Wtype-limits"
CFLAGS+=" -Wwrite-strings"
# clang only: CFLAGS+=" -fdiagnostics-color=auto"

# warnings disabled on purpose
CFLAGS+=" -Wno-unused-parameter"
CFLAGS+=" -Wno-unused-function"
CFLAGS+=" -Wno-deprecated-declarations"

# should be removed and the code fixed
CFLAGS+=" -Wno-incompatible-pointer-types-discards-qualifiers"
CFLAGS+=" -Wno-missing-field-initializers"

# fails on warning -- leave it diesabled; not all flags are
# compatible with both compiler (gcc/clang)
#CFLAGS+=" -Werror"

export CFLAGS
./autogen.sh "$@"

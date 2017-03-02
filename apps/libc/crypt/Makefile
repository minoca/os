################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Crypt Library
#
#   Abstract:
#
#       This module contains the crypt library, which contains the C library
#       functions like crypt, encrypt, fcrypt, and setkey.
#
#   Author:
#
#       Evan Green 6-Mar-2015
#
#   Environment:
#
#       User Mode C Library
#
################################################################################

BINARY = libcrypt.so.1

BINARYTYPE = so

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = crypt.o          \

DYNLIBS = $(BINROOT)/libc.so.1           \

EXTRA_LDFLAGS += -nostdlib

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/lib
	@cp -p $(BINROOT)/$(BINARY) $(BINROOT)/skel/lib


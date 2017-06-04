################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       UEFI Runtime Library
#
#   Abstract:
#
#       This file is responsible for building the core UEFI Runtime support,
#       which is compiled into most platform runtime images.
#
#   Author:
#
#       Evan Green 18-Mar-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = rtlib.a

BINARYTYPE = klibrary

INCLUDES += $(SRCROOT)/os/uefi/include;$(SRCDIR)/..;

OBJS = crc32.o      \
       div.o        \
       init.o       \
       time.o       \
       util.o       \
       var.o        \

include $(SRCROOT)/os/minoca.mk

VPATH += $(SRCDIR)/..:

CFLAGS += -fshort-wchar


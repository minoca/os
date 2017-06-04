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
#       Architecture support
#
#   Abstract:
#
#       This module contains architecure-specific UEFI core support functions.
#
#   Author:
#
#       Evan Green 27-Mar-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = uefiarch.a

BINARYTYPE = klibrary

INCLUDES += $(SRCROOT)/os/uefi/include;$(SRCROOT)/os/uefi/core;

X86_OBJS = x86/archlib.o    \
           x86/archsup.o    \
           x86/ioport.o     \
           x86/regacces.o   \

ARMV7_OBJS = armv7/archlib.o    \
             armv7/archsup.o    \
             armv7/regacces.o   \

ARMV6_OBJS = armv7/archlib.o    \
             armv6/archsup.o    \
             armv7/regacces.o   \

EXTRA_SRC_DIRS = x86 armv7 armv6

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar


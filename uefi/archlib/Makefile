################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
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

BINARYTYPE = library

INCLUDES += $(SRCROOT)/os/uefi/inc;$(SRCROOT)/os/uefi/core;

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


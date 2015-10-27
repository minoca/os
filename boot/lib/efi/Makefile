################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       UEFI Boot Library
#
#   Abstract:
#
#       This library contains the UEFI boot library.
#
#   Author:
#
#       Evan Green 19-Feb-2014
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = bootefi.a

BINARYTYPE = library

include $(SRCDIR)/../sources

INCLUDES += $(SRCDIR)/../inc;$(SRCDIR)/..;

OBJS += dbgser.o   \
        disk.o     \
        fwapi.o    \
        memory.o   \
        time.o     \
        util.o     \
        video.o    \

X86_OBJS += x86/efia.o      \
            x86/efiarch.o

ARMV7_OBJS += armv7/efia.o      \
              armv7/efiarch.o   \

ARMV6_OBJS += armv7/efia.o      \
              armv7/efiarch.o   \

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk

EXTRA_CFLAGS += -fshort-wchar


################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Runtime Core
#
#   Abstract:
#
#       This module implements the UEFI core runtime support, which only
#       supports setting the virtual address map. It is implemented as a
#       separate runtime binary to avoid the paradox of ExitBootServices
#       and SetVirtualAddressMap trying to tear down or change themselves.
#
#   Author:
#
#       Evan Green 10-Mar-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = rtbase.elf

BINARYTYPE = staticapp

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/uefi/inc;..;

OBJS = ../crc32.o \
       runtime.o  \

VPATH += $(BINROOT):

ENTRY = EfiRuntimeDriverEntry

LDFLAGS += -pie -nodefaultlibs -nostartfiles -nostdlib

ifeq ($(ARCH), armv7)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/inc/link_arm.x
endif

ifeq ($(ARCH), armv6)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/inc/link_arm.x
endif

TARGETLIBS = $(INTLIB)/os/uefi/archlib/uefiarch.a       \

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: rtbase

rtbase: $(BINARY)
	@cd $(BINROOT) && elfconv -o $@ -t efiruntimedriver $(BINARY)

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar


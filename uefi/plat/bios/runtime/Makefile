################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       PC/AT UEFI Runtime firmware
#
#   Abstract:
#
#       This module implements the PC/AT BIOS runtime firmware, which continues
#       to run even after ExitBootServices. It is never unloaded.
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

BINARY = biosrt.elf

BINARYTYPE = staticapp

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/uefi/inc;$(SRCDIR)/..;

BINPLACE = bin

OBJS += reboot.o    \
        runtime.o   \

ENTRY = EfiRuntimeCoreEntry

LDFLAGS += -pie -nodefaultlibs -nostartfiles -nostdlib

ifeq ($(ARCH), armv7)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/inc/link_arm.x
endif

TARGETLIBS = $(INTLIB)/os/uefi/core/rtlib/rtlib.a       \
             $(INTLIB)/os/uefi/archlib/uefiarch.a       \

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild:
	@cd $(BINROOT) && elfconv -o biosrt -t efiruntimedriver $(BINARY)

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar


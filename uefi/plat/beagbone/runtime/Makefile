################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       BeagleBone UEFI Runtime
#
#   Abstract:
#
#       This module implements the BeagleBone runtime firmware, which continues
#       to be loaded and provide services to the OS kernel even after the boot
#       environment has been destroyed. It is never unloaded.
#
#   Author:
#
#       Evan Green 6-Jan-2015
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = bbonert.elf

BINARYTYPE = staticapp

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/uefi/inc;$(SRCDIR)/..;

BINPLACE = bin

OBJS += reboot.o    \
        runtime.o   \

VPATH += $(BINROOT):

ENTRY = EfiRuntimeCoreEntry

LDFLAGS += -pie -Wl,--no-wchar-size-warning
LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

ifeq ($(ARCH), armv7)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/inc/link_arm.x
endif

TARGETLIBS = $(INTLIB)/os/uefi/core/rtlib/rtlib.a       \
             $(INTLIB)/os/uefi/archlib/uefiarch.a       \

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: bbonert

bbonert: $(BINARY)
	@cd $(BINROOT) && elfconv -o $@ -t efiruntimedriver $(BINARY)


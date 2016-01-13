################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Integrator UEFI Runtime
#
#   Abstract:
#
#       This module implements the Integrator runtime firmware, which continues
#       running even after boot services have been torn down. It is never
#       unloaded.
#
#   Author:
#
#       Evan Green 7-Apr-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = integrt.elf

BINARYTYPE = staticapp

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/uefi/inc;$(SRCDIR)/..;

BINPLACE = bin

OBJS += rtc.o       \
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
             $(INTLIB)/os/uefi/dev/pl031/pl031.a        \

include $(SRCROOT)/os/minoca.mk

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: integrt

integrt: $(BINARY)
	@cd $(BINROOT) && elfconv -o $@ -t efiruntimedriver $(BINARY)

CFLAGS += -fshort-wchar


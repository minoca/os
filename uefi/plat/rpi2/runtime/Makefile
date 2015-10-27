################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Raspberry Pi 2 UEFI Runtime
#
#   Abstract:
#
#       This module implements the Raspberry Pi 2 runtime firmware, which
#       continues running even after boot services have been torn down. It is
#       never unloaded.
#
#   Author:
#
#       Chris Stevens 17-Mar-2015
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = rpi2rt.elf

BINARYTYPE = staticapp

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/uefi/inc;$(SRCDIR)/..;

BINPLACE = bin

OBJS += runtime.o   \

VPATH += $(BINROOT):

ENTRY = EfiRuntimeCoreEntry

LDFLAGS += -pie -Wl,--no-wchar-size-warning
LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

ifeq ($(ARCH), armv7)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/inc/link_arm.x
endif

TARGETLIBS = $(OBJROOT)/os/uefi/core/rtlib/rtlib.a \
             $(OBJROOT)/os/uefi/archlib/uefiarch.a \

include $(SRCROOT)/os/minoca.mk

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: rpi2rt

postbuild: rpi2rt.ffs

rpi2rt.ffs: rpi2rt
	@genffs -s -i $< -r EFI_SECTION_PE32 \
	    -i $< -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER -o $@

rpi2rt: $(BINARY)
	@echo Converting - $(notdir $<)
	@elfconv -o $@ -t efiruntimedriver $<

CFLAGS += -fshort-wchar


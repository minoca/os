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

INCLUDES += $(SRCROOT)/os/uefi/include;$(SRCDIR)/..;

OBJS += reboot.o    \
        runtime.o   \

ENTRY = EfiRuntimeCoreEntry

LDFLAGS += -pie -nodefaultlibs -nostartfiles -nostdlib

ifeq ($(ARCH), armv7)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/include/link_arm.x
endif

TARGETLIBS = $(OBJROOT)/os/uefi/core/rtlib/rtlib.a       \
             $(OBJROOT)/os/uefi/archlib/uefiarch.a       \

include $(SRCROOT)/os/minoca.mk

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: biosrt.ffs

biosrt.ffs: biosrt
	@genffs -s -i $< -r EFI_SECTION_PE32 \
	    -i $< -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER -o $@

biosrt: $(BINARY)
	@echo Converting - $(notdir $<)
	@elfconv -o $@ -t efiruntimedriver $<

CFLAGS += -fshort-wchar


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
#       PandaBoard UEFI Runtime
#
#   Abstract:
#
#       This module implements the PandaBoard runtime firmware, which continues
#       to be loaded and provide services to the OS kernel even after the boot
#       environment has been destroyed. It is never unloaded.
#
#   Author:
#
#       Evan Green 19-Mar-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = pandart.elf

BINARYTYPE = staticapp

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/uefi/include;$(SRCDIR)/..;

OBJS += i2c.o       \
        pmic.o      \
        rtc.o       \
        reboot.o    \
        runtime.o   \

VPATH += $(BINROOT):

ENTRY = EfiRuntimeCoreEntry

LDFLAGS += -pie -Wl,--no-wchar-size-warning
LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

ifeq ($(ARCH), armv7)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/include/link_arm.x
endif

TARGETLIBS = $(OBJROOT)/os/uefi/core/rtlib/rtlib.a       \
             $(OBJROOT)/os/uefi/archlib/uefiarch.a       \

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: pandart.ffs

pandart.ffs: pandart
	@genffs -s -i $< -r EFI_SECTION_PE32 \
	    -i $< -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER -o $@

pandart: $(BINARY)
	@echo Converting - $(notdir $<)
	@elfconv -o $@ -t efiruntimedriver $<


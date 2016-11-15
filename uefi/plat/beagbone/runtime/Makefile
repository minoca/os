################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

INCLUDES += $(SRCROOT)/os/uefi/include;$(SRCDIR)/..;

OBJS += reboot.o    \
        rtc.o       \
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

postbuild: bbonert

postbuild: bbonert.ffs

bbonert.ffs: bbonert
	@genffs -s -i $< -r EFI_SECTION_PE32 \
	    -i $< -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER -o $@

bbonert: $(BINARY)
	@echo Converting - $(notdir $<)
	@elfconv -o $@ -t efiruntimedriver $<


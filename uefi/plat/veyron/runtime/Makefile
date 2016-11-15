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
#       Veyron UEFI Runtime
#
#   Abstract:
#
#       This module implements the RK3288 Veyron runtime firmware, which
#       continues to be loaded and provide services to the OS kernel even
#       after the boot environment has been destroyed. It is never unloaded.
#
#   Author:
#
#       Evan Green 10-Jul-2015
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = veyronrt.elf

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
## Convert it to a PE image.
##

postbuild: veyronrt.ffs

veyronrt.ffs: veyronrt
	@genffs -s -i $< -r EFI_SECTION_PE32 \
	    -i $< -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER -o $@

veyronrt: $(BINARY)
	@echo Converting - $(notdir $<)
	@elfconv -o $@ -t efiruntimedriver $<

CFLAGS += -fshort-wchar


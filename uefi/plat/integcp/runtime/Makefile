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

INCLUDES += $(SRCROOT)/os/uefi/include;$(SRCDIR)/..;

OBJS += rtc.o       \
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
             $(OBJROOT)/os/uefi/dev/pl031/pl031.a        \

include $(SRCROOT)/os/minoca.mk

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: integrt.ffs

integrt.ffs: integrt
	@genffs -s -i $< -r EFI_SECTION_PE32 \
	    -i $< -r EFI_SECTION_USER_INTERFACE -t EFI_FV_FILETYPE_DRIVER -o $@

integrt: $(BINARY)
	@elfconv -o $@ -t efiruntimedriver $<

CFLAGS += -fshort-wchar


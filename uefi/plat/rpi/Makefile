################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Raspberry Pi UEFI Firmware
#
#   Abstract:
#
#       This module implements UEFI firmware for the Raspberry Pi.
#
#   Author:
#
#       Chris Stevens 31-Dec-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

include $(SRCDIR)/../common

BINARY := rpifw.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

OBJS += debug.o             \
        fwvol.o             \
        intr.o              \
        main.o              \
        memmap.o            \
        rpifwv.o            \
        ramdenum.o          \
        smbios.o            \
        timer.o             \

ARMV6_OBJS = armv6/entry.o  \

TEXT_ADDRESS := 0x00008000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/pl11/pl11.a       \
              $(OBJROOT)/os/uefi/dev/sd/core/sd.a      \
              $(OBJROOT)/os/uefi/dev/bcm2709/bcm2709.a \
              $(OBJROOT)/os/uefi/core/emptyrd/emptyrd.a

EXTRA_SRC_DIRS = armv6

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), rpifw.elf)

DIRS = acpi     \
       blobs    \
       runtime

endif

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/rpirt.ffs acpi/acpi.ffs

rpifwv.o: rpifwv

rpifwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

runtime/rpirt.ffs: runtime
acpi/acpi.ffs: acpi

postbuild: rpifw

rpifw: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@
	@cp -p $@ $(BINROOT)/


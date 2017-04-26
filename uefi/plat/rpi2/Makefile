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
#       Raspberry Pi 2 UEFI Firmware
#
#   Abstract:
#
#       This module implements UEFI firmware for the Raspberry Pi 2.
#
#   Author:
#
#       Chris Stevens 19-Mar-2015
#
#   Environment:
#
#       Firmware
#
################################################################################

include $(SRCDIR)/../common

BINARY := rpi2fw.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

OBJS += debug.o              \
        fwvol.o              \
        intr.o               \
        main.o               \
        memmap.o             \
        rpi2fwv.o            \
        ramdenum.o           \
        smbios.o             \
        smp.o                \
        timer.o              \

ARMV7_OBJS = armv7/entry.o  \
             armv7/smpa.o   \
             armv7/timera.o \

TEXT_ADDRESS := 0x00008000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/pl11/pl11.a       \
              $(OBJROOT)/os/uefi/dev/sd/core/sd.a      \
              $(OBJROOT)/os/uefi/dev/bcm2709/bcm2709.a \
              $(OBJROOT)/os/uefi/core/emptyrd/emptyrd.a

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), rpi2fw.elf)

DIRS = acpi     \
       blobs    \
       runtime

endif

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/rpi2rt.ffs acpi/acpi.ffs

rpi2fwv.o: rpi2fwv

rpi2fwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

runtime/rpi2rt.ffs: runtime
acpi/acpi.ffs: acpi

postbuild: rpi2fw

rpi2fw: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@
	@cp -p $@ $(BINROOT)/

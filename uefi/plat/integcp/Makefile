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
#       Integrator/CP UEFI Firmware
#
#   Abstract:
#
#       This module implements UEFI firmware for the ARM Integrator/CP.
#
#   Author:
#
#       Evan Green 4-Apr-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

include $(SRCDIR)/../common

BINARY := integfw.elf

BINARYTYPE = staticapp

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK_O = ramdisk.o

OBJS += debug.o                 \
        fwvol.o                 \
        intr.o                  \
        main.o                  \
        memmap.o                \
        integfwv.o              \
        ramdenum.o              \
        $(RAMDISK_O)            \
        serial.o                \
        smbios.o                \
        timer.o                 \
        video.o                 \

ARMV7_OBJS = armv7/entry.o  \

TEXT_ADDRESS := 0x80100000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/pl11/pl11.a     \
              $(OBJROOT)/os/uefi/dev/pl110/pl110.a   \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), integfw.elf)

BINPLACE = bin

RAMDISK_O = $(OBJROOT)/os/uefi/core/emptyrd/emptyrd.a

DIRS = acpi     \
       runtime

endif

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/integrt.ffs acpi/acpi.ffs

integfwv.o: integfwv
ramdisk.o: ramdisk

integfwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

runtime/integrt.ffs: runtime
acpi/acpi.ffs: acpi

##
## Manually copy to binroot, since the binplace variable isn't set for the
## second iteration of this build. Setting it causes all images to be rebuilt
## every time.
##

postbuild: $(BINARY)
	@cp -p $^ $(BINROOT)/


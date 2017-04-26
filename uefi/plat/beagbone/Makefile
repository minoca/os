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
#       BeagleBone UEFI Firmware
#
#   Abstract:
#
#       This module implements UEFI firmware for the TI BeagleBone Black.
#
#   Author:
#
#       Evan Green 19-Dec-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

include $(SRCDIR)/../common

BINARY := bbonefw.elf

BINARYTYPE = staticapp

BINPLACE = bin

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK_O = ramdisk.o

OBJS += clock.o                 \
        debug.o                 \
        fwvol.o                 \
        i2c.o                   \
        intr.o                  \
        main.o                  \
        memmap.o                \
        bbonefwv.o              \
        ramdenum.o              \
        $(RAMDISK_O)            \
        sd.o                    \
        serial.o                \
        smbios.o                \
        timer.o                 \
        video.o                 \

ARMV7_OBJS = armv7/entry.o  \

TEXT_ADDRESS := 0x82000000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/sd/core/sd.a            \
              $(OBJROOT)/os/uefi/dev/omapuart/omapuart.a     \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), bbonefw.elf)

RAMDISK_O = $(OBJROOT)/os/uefi/core/emptyrd/emptyrd.a

DIRS = acpi     \
       init     \
       runtime

endif

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/bbonert.ffs acpi/acpi.ffs

bbonefwv.o: bbonefwv

bbonefwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

ramdisk.o: ramdisk
runtime/bbonert.ffs: runtime
acpi/acpi.ffs: acpi

##
## Handle any post build steps to fix up the firmware images.
##

ifeq ($(BINARY), bbonefw.elf)

postbuild: bbonefw bbonefw.bin

bbonefw: bbonefw.bin
	@echo Generating - $@
	@mkuboot -c -a arm -f legacy -l $(TEXT_ADDRESS) -e $(TEXT_ADDRESS) -o $@ $^
	@cp -p $@ $(BINROOT)/

bbonefw.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@

endif


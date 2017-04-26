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
#       PandaBoard UEFI Firmware
#
#   Abstract:
#
#       This module implements UEFI firmware for the TI PandaBoard.
#
#   Author:
#
#       Evan Green 26-Feb-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

include $(SRCDIR)/../common

BINARY := pandafw.elf

BINARYTYPE = staticapp

ifeq ($(BINARY), pandafw.elf)

endif

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK_O = ramdisk.o

OBJS += debug.o                 \
        fwvol.o                 \
        init/id.o               \
        intr.o                  \
        main.o                  \
        memmap.o                \
        omap4usb.o              \
        pandafwv.o              \
        ramdenum.o              \
        $(RAMDISK_O)            \
        sd.o                    \
        serial.o                \
        smbios.o                \
        smp.o                   \
        timer.o                 \
        video.o                 \

ARMV7_OBJS = armv7/entry.o  \
             armv7/smpa.o   \

TEXT_ADDRESS := 0x82000000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/gic/gic.a               \
              $(OBJROOT)/os/uefi/dev/sd/core/sd.a            \
              $(OBJROOT)/os/uefi/dev/omap4/omap4.a           \
              $(OBJROOT)/os/uefi/dev/omapuart/omapuart.a     \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), pandafw.elf)

BINPLACE = bin

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

FWVOL_IMAGES = $(RTBASE) runtime/pandart.ffs acpi/acpi.ffs

pandafwv.o: pandafwv
ramdisk.o: ramdisk

pandafwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

runtime/pandart.ffs: runtime
acpi/acpi.ffs: acpi

##
## Handle any post build steps to fix up the firmware images.
##

ifeq ($(BINARY), pandafw.elf)

postbuild: pandafw pandafw.bin

pandafw: pandafw.bin
	@echo Generating - $@
	@mkuboot -c -a arm -f legacy -l $(TEXT_ADDRESS) -e $(TEXT_ADDRESS) -o $@ $^
	@cp -p $@ $(BINROOT)/

pandafw.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@

endif

ifeq ($(BINARY), pandafw_usb.elf)

postbuild: pandafw_usb pandafw_usb.bin

pandafw_usb: pandafw_usb.bin
	@echo Generating - $@
	@mkuboot -c -a arm -f legacy -l $(TEXT_ADDRESS) -e $(TEXT_ADDRESS) -o $@ $^
	@cp -p $@ $(BINROOT)/pandausb.img

pandafw_usb.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@

endif

##
## Manually copy to binroot, since the binplace variable isn't set for the
## second iteration of the USB build. Setting it causes all images to be
## rebuilt every time.
##

postbuild: $(BINARY)
	@cp -p $(BINARY) $(BINROOT)/


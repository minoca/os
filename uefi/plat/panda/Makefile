################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

FIRMWARE_VERSION_MAJOR = 1
FIRMWARE_VERSION_MINOR = 0

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK = emptyrd
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

TEXT_ADDRESS = 0x82000000

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

RAMDISK_O = emptyrd.o

DIRS = acpi     \
       init     \
       runtime

endif

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## Set the build date and version to a hardcoded value to avoid the SMBIOS
## table changing from build to build. The automated tests checksum the whole
## table to get a machine ID, so if the dates or versions change then each
## new firmware iteration looks like an entirely new machine.
##

#FIRMWARE_BUILD_DATE := "$(shell date "+%m/%d/%Y")"
#FIRMWARE_VERSION_STRING := \
#    "$(FIRMWARE_VERSION_MAJOR).$(FIRMWARE_VERSION_MINOR).$(REVISION)"

FIRMWARE_BUILD_DATE := "08/15/2014"
FIRMWARE_VERSION_STRING := \
    "$(FIRMWARE_VERSION_MAJOR).$(FIRMWARE_VERSION_MINOR)"

EXTRA_CPPFLAGS += -DFIRMWARE_BUILD_DATE=\"$(FIRMWARE_BUILD_DATE)\" \
                  -DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR) \
                  -DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR) \
                  -DFIRMWARE_VERSION_STRING=\"$(FIRMWARE_VERSION_STRING)\"

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/pandart.ffs acpi/acpi.ffs

pandafwv.o: pandafwv
	@echo Objectifying - $(notdir $<)
	@$(OBJCOPY) -B $(BFD_ARCH) -I binary -O $(OBJ_FORMAT) $< $@

pandafwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

$(RAMDISK_O): $(RAMDISK)
	@echo Objectifying - $(notdir $<)
	@cp $^ ramdisk
	@$(OBJCOPY) -I binary -O $(OBJ_FORMAT) -B $(BFD_ARCH) ramdisk $@

emptyrd:
	@echo Minoca > $@

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


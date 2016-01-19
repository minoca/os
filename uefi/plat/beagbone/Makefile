################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
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

FIRMWARE_VERSION_MAJOR = 1
FIRMWARE_VERSION_MINOR = 0

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK = emptyrd
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

TEXT_ADDRESS = 0x82000000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/sd/core/sd.a            \
              $(OBJROOT)/os/uefi/dev/omapuart/omapuart.a     \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), bbonefw.elf)

RAMDISK_O = emptyrd.o

DIRS = acpi     \
       init     \
       runtime

endif

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

FIRMWARE_BUILD_DATE := "$(shell date "+%m/%d/%Y")"
FIRMWARE_VERSION_STRING := \
    "$(FIRMWARE_VERSION_MAJOR).$(FIRMWARE_VERSION_MINOR).$(REVISION)"

EXTRA_CPPFLAGS += -DFIRMWARE_BUILD_DATE=\"$(FIRMWARE_BUILD_DATE)\" \
                  -DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR) \
                  -DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR) \
                  -DFIRMWARE_VERSION_STRING=\"$(FIRMWARE_VERSION_STRING)\"

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/bbonert.ffs acpi/acpi.ffs

bbonefwv.o: bbonefwv
	@echo Objectifying - $(notdir $<)
	@$(OBJCOPY) -B $(BFD_ARCH) -I binary -O $(OBJ_FORMAT) $< $@

bbonefwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

$(RAMDISK_O): $(RAMDISK)
	@echo Objectifying - $(notdir $<)
	@cp $^ ramdisk
	@$(OBJCOPY) -I binary -O $(OBJ_FORMAT) -B $(BFD_ARCH) ramdisk $@

emptyrd:
	@echo Minoca > $@

runtime/bbonert.ffs: runtime

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


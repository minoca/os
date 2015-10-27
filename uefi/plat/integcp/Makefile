################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

FIRMWARE_VERSION_MAJOR = 1
FIRMWARE_VERSION_MINOR = 0

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK = emptyrd
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

TEXT_ADDRESS = 0x80100000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/pl11/pl11.a     \
              $(OBJROOT)/os/uefi/dev/pl110/pl110.a   \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), integfw.elf)

BINPLACE = bin

RAMDISK_O = emptyrd.o

DIRS = acpi     \
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

FWVOL_IMAGES = $(RTBASE) runtime/integrt.ffs acpi/acpi.ffs

integfwv.o: integfwv
	@echo Objectifying - $(notdir $<)
	@$(OBJCOPY) -B $(BFD_ARCH) -I binary -O $(OBJ_FORMAT) $< $@

integfwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

##
## Copy the ramdisk name to a file named simply "ramdisk" so that the objcopy
## generated symbols will be _binary_ramdisk_start.
##

$(RAMDISK_O): $(RAMDISK)
	@echo Objectifying - $(notdir $<)
	@cp $< ramdisk
	@$(OBJCOPY) -I binary -O elf32-littlearm -B arm ramdisk $@

emptyrd:
	@echo Minoca > $@

runtime/integrt.ffs: runtime

##
## Manually copy to binroot, since the binplace variable isn't set for the
## second iteration of this build. Setting it causes all images to be rebuilt
## every time.
##

postbuild: $(BINARY)
	@cp -p $^ $(BINROOT)/


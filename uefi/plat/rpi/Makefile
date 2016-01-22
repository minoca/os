################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

FIRMWARE_VERSION_MAJOR = 1
FIRMWARE_VERSION_MINOR = 0

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK = emptyrd
RAMDISK_O = ramdisk.o

OBJS += debug.o             \
        fwvol.o             \
        intr.o              \
        main.o              \
        memmap.o            \
        rpifwv.o            \
        ramdenum.o          \
        $(RAMDISK_O)        \
        smbios.o            \
        timer.o             \

ARMV6_OBJS = armv6/entry.o  \

TEXT_ADDRESS = 0x00008000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/pl11/pl11.a       \
              $(OBJROOT)/os/uefi/dev/sd/core/sd.a      \
              $(OBJROOT)/os/uefi/dev/bcm2709/bcm2709.a \

EXTRA_SRC_DIRS = armv6

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), rpifw.elf)

RAMDISK_O = emptyrd.o

DIRS = acpi     \
       blobs    \
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

FIRMWARE_BUILD_DATE := "01/01/2015"
FIRMWARE_VERSION_STRING := \
    "$(FIRMWARE_VERSION_MAJOR).$(FIRMWARE_VERSION_MINOR)"

EXTRA_CPPFLAGS += -DFIRMWARE_BUILD_DATE=\"$(FIRMWARE_BUILD_DATE)\" \
                  -DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR) \
                  -DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR) \
                  -DFIRMWARE_VERSION_STRING=\"$(FIRMWARE_VERSION_STRING)\"

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/rpirt.ffs acpi/acpi.ffs

rpifwv.o: rpifwv
	@echo Objectifying - $(notdir $<)
	@$(OBJCOPY) -B $(BFD_ARCH) -I binary -O $(OBJ_FORMAT) $< $@

rpifwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

$(RAMDISK_O): $(RAMDISK)
	@echo Objectifying - $(notdir $<)
	@cp $^ ramdisk
	@$(OBJCOPY) -I binary -O $(OBJ_FORMAT) -B $(BFD_ARCH) ramdisk $@

emptyrd:
	@echo Minoca > $@

runtime/rpirt.ffs: runtime
acpi/acpi.ffs: acpi

postbuild: rpifw

rpifw: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@
	@cp -p $@ $(BINROOT)/


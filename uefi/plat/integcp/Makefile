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

include ../common

BINARY := integfw.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

FIRMWARE_VERSION_MAJOR = 1
FIRMWARE_VERSION_MINOR = 0

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

RAMDISK = $(OBJROOT)/$(THISDIR)/emptyrd
RAMDISK_O = ramdisk.o

OBJS += debug.o                 \
        fwvol.o                 \
        intr.o                  \
        main.o                  \
        memmap.o                \
        $(BINROOT)/integfwv.o   \
        ramdenum.o              \
        $(RAMDISK_O)            \
        serial.o                \
        smbios.o                \
        timer.o                 \
        video.o                 \

ARMV7_OBJS = armv7/entry.o  \

TEXT_ADDRESS = 0x80100000

TARGETLIBS += $(INTLIB)/os/uefi/dev/pl11/pl11.a     \
              $(INTLIB)/os/uefi/dev/pl110/pl110.a   \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), integfw.elf)

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

FWVOL_IMAGES = rtbase runtime/integrt.elf

$(BINROOT)/integfwv.o: $(FWVOL_IMAGES)
	@sh $(SRCDIR)/genfwvol.sh $(OBJCOPY)

$(RAMDISK_O): $(RAMDISK)
	@cd $(OBJROOT)/$(THISDIR) && \
	    cp $^ ramdisk && \
	    $(OBJCOPY) -I binary -O elf32-littlearm -B armv5t ramdisk $@

$(OBJROOT)/$(THISDIR)/emptyrd:
	@echo Minoca > $@

runtime/integrt.elf: runtime


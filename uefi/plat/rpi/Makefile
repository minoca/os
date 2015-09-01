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

RAMDISK = $(OBJROOT)/$(THISDIR)/emptyrd
RAMDISK_O = ramdisk.o

OBJS += debug.o             \
        fwvol.o             \
        intr.o              \
        main.o              \
        memmap.o            \
        $(BINROOT)/rpifwv.o \
        ramdenum.o          \
        $(RAMDISK_O)        \
        smbios.o            \
        timer.o             \

ARMV6_OBJS = armv6/entry.o  \

TEXT_ADDRESS = 0x00008000

TARGETLIBS += $(INTLIB)/os/uefi/dev/pl11/pl11.a       \
              $(INTLIB)/os/uefi/dev/sd/core/sd.a      \
              $(INTLIB)/os/uefi/dev/bcm2709/bcm2709.a \

EXTRA_SRC_DIRS = armv6

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), rpifw.elf)

RAMDISK_O = emptyrd.o

DIRS = acpi     \
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

FWVOL_IMAGES = rtbase runtime/rpirt.elf

$(BINROOT)/rpifwv.o: $(FWVOL_IMAGES)
	@sh $(SRCDIR)/genfwvol.sh $(OBJCOPY)

$(RAMDISK_O): $(RAMDISK)
	@cd $(OBJROOT)/$(THISDIR) && \
	    cp $^ ramdisk && \
	    $(OBJCOPY) -I binary -O elf32-littlearm -B armv5t ramdisk $@

$(OBJROOT)/$(THISDIR)/emptyrd:
	@echo Minoca > $@

runtime/rpirt.elf: runtime


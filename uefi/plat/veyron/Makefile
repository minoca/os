################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Veyron UEFI Firmware
#
#   Abstract:
#
#       This module implements UEFI firmware for the ASUS C201 (Veyron) SoC.
#
#   Author:
#
#       Chris Stevens 6-Jul-2015
#
#   Environment:
#
#       Firmware
#
################################################################################

include ../common

BINARY := veyronfw.elf

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
        ramdenum.o              \
        $(RAMDISK_O)            \
        sd.o                    \
        serial.o                \
        smbios.o                \
        smp.o                   \
        timer.o                 \
        usb.o                   \
        $(BINROOT)/veyrnfwv.o   \
        video.o                 \

ARMV7_OBJS = armv7/entry.o  \
             armv7/smpa.o   \

TEXT_ADDRESS = 0x020000A4

TARGETLIBS += $(INTLIB)/os/uefi/dev/gic/gic.a               \
              $(INTLIB)/os/uefi/dev/ns16550/ns16550.a       \
              $(INTLIB)/os/uefi/dev/sd/core/sd.a            \
              $(INTLIB)/os/uefi/dev/sd/rk/sdrk.a            \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), veyronfw.elf)

RAMDISK_O = emptyrd.o

DIRS = acpi     \
       fwbuild  \
       runtime  \

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
## Don't bother to page align data since the text segment is loaded at a
## specific and unaligned place.
##

EXTRA_LDFLAGS += -Wl,--nmagic

KEYBLOCK = $(SRCROOT)/os/uefi/plat/veyron/veyron.kbk
PRIVATE_KEY = $(SRCROOT)/os/uefi/plat/veyron/veyron.pem

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = rtbase runtime/veyronrt.elf

$(BINROOT)/veyrnfwv.o: $(FWVOL_IMAGES)
	@sh $(SRCDIR)/genfwvol.sh $(OBJCOPY)

$(RAMDISK_O): $(RAMDISK)
	@cd $(OBJROOT)/$(THISDIR) && \
	    cp $^ ramdisk && \
	    $(OBJCOPY) -I binary -O elf32-littlearm -B armv5t ramdisk $@

$(OBJROOT)/$(THISDIR)/emptyrd:
	@echo Minoca > $@

runtime/veyronrt.elf: runtime

##
## Handle any post build steps to fix up the firmware images.
##

postbuild: veyronfw veyronfw.img veyronfw.bin

veyronfw: veyronfw.img | fwbuild
	@cd $(BINROOT) && veyrnfwb $(TEXT_ADDRESS) $(KEYBLOCK) $(PRIVATE_KEY) $^ $@

veyronfw.img: veyronfw.bin
	@cd $(BINROOT) && mkuboot -c -l $(TEXT_ADDRESS) -e $(TEXT_ADDRESS) \
	     -a arm -f fit -o $@ $^

veyronfw.bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


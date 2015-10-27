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

include $(SRCDIR)/../common

BINARY := veyronfw.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

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
        ramdenum.o              \
        $(RAMDISK_O)            \
        sd.o                    \
        serial.o                \
        smbios.o                \
        smp.o                   \
        timer.o                 \
        usb.o                   \
        veyrnfwv.o              \
        video.o                 \

ARMV7_OBJS = armv7/entry.o  \
             armv7/smpa.o   \

TEXT_ADDRESS = 0x020000A4

TARGETLIBS += $(OBJROOT)/os/uefi/dev/gic/gic.a               \
              $(OBJROOT)/os/uefi/dev/ns16550/ns16550.a       \
              $(OBJROOT)/os/uefi/dev/sd/core/sd.a            \
              $(OBJROOT)/os/uefi/dev/sd/dwc/sddwc.a          \

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

FWVOL_IMAGES = $(RTBASE) runtime/veyronrt.ffs acpi/acpi.ffs

veyrnfwv.o: veyrnfwv
	@echo Objectifying - $(notdir $<)
	@$(OBJCOPY) -B $(BFD_ARCH) -I binary -O $(OBJ_FORMAT) $< $@

veyrnfwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

$(RAMDISK_O): $(RAMDISK)
	@echo Objectifying - $(notdir $<)
	@cp $^ ramdisk
	@$(OBJCOPY) -I binary -O $(OBJ_FORMAT) -B $(BFD_ARCH) ramdisk $@

emptyrd:
	@echo Minoca > $@

runtime/veyronrt.ffs: runtime

##
## Handle any post build steps to fix up the firmware images.
##

postbuild: veyronfw

veyronfw: veyronfw.img | fwbuild
	@echo Generating - $@
	@fwbuild/veyrnfwb $(TEXT_ADDRESS) $(KEYBLOCK) $(PRIVATE_KEY) $^ $@
	@cp -p $@ $(BINROOT)/

veyronfw.img: veyronfw.bin
	@echo Generating - $@
	@mkuboot -c -l $(TEXT_ADDRESS) -e $(TEXT_ADDRESS) -a arm -f fit -o $@ $^

veyronfw.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@


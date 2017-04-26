################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

LDFLAGS += -Wl,--no-wchar-size-warning -nodefaultlibs -nostartfiles -nostdlib

OBJS += debug.o                 \
        fwvol.o                 \
        intr.o                  \
        main.o                  \
        memmap.o                \
        ramdenum.o              \
        sd.o                    \
        serial.o                \
        smbios.o                \
        smp.o                   \
        timer.o                 \
        usb.o                   \
        veyronfwv.o             \
        video.o                 \

ARMV7_OBJS = armv7/entry.o   \
             armv7/minttbl.o \
             armv7/smpa.o    \

TEXT_ADDRESS := 0x020000A4

TARGETLIBS += $(OBJROOT)/os/uefi/dev/gic/gic.a               \
              $(OBJROOT)/os/uefi/dev/ns16550/ns16550.a       \
              $(OBJROOT)/os/uefi/dev/sd/core/sd.a            \
              $(OBJROOT)/os/uefi/dev/sd/dwc/sddwc.a          \
              $(OBJROOT)/os/uefi/core/emptyrd/emptyrd.a      \

EXTRA_SRC_DIRS = armv7

##
## Only build the directories if the binary hasn't been forced on the command
## line.
##

ifeq ($(BINARY), veyronfw.elf)

DIRS = acpi     \
       fwbuild  \
       runtime  \

endif

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

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

veyronfwv.o: veyronfwv

veyronfwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

runtime/veyronrt.ffs: runtime
acpi/acpi.ffs: acpi

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


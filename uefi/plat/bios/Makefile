################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       BIOS UEFI Firmware
#
#   Abstract:
#
#       This module implements a UEFI-compatible firmware layer on top of a
#       legacy PC/AT BIOS.
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

BINARY := biosfw.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

OBJS += acpi.o      \
        bioscall.o  \
        biosfwv.o   \
        disk.o      \
        debug.o     \
        fwvol.o     \
        intr.o      \
        main.o      \
        memmap.o    \
        timer.o     \
        video.o     \

X86_OBJS = x86/entry.o      \
           x86/realmexe.o   \

TEXT_ADDRESS = 0x100000

TARGETLIBS += $(OBJROOT)/os/uefi/dev/ns16550/ns16550.a       \

EXTRA_SRC_DIRS = x86

DIRS = runtime

include $(SRCROOT)/os/minoca.mk

postbuild: biosfw.bin

biosfw.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $< $@
	@cp -p $@ $(BINROOT)/

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = $(RTBASE) runtime/biosrt.ffs

biosfwv.o: biosfwv
	@echo Objectifying - $(notdir $<)
	@$(OBJCOPY) -B $(BFD_ARCH) -I binary -O $(OBJ_FORMAT) $< $@

biosfwv: $(FWVOL_IMAGES)
	@echo Generating - $@
	@genfv -o $@ $^

$(RAMDISK_O): $(RAMDISK)
	@echo Objectifying - $(notdir $<)
	@cp $^ ramdisk
	@$(OBJCOPY) -I binary -O $(OBJ_FORMAT) -B $(BFD_ARCH) ramdisk $@

runtime/biosrt.ffs: runtime


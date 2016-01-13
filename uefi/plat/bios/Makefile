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

include ../common

BINARY := biosfw.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

OBJS += acpi.o      \
        bioscall.o  \
        $(BINROOT)/biosfwv.o   \
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

TARGETLIBS += $(INTLIB)/os/uefi/dev/ns16550/ns16550.a       \

EXTRA_SRC_DIRS = x86

DIRS = runtime

include $(SRCROOT)/os/minoca.mk

postbuild: $(BINARY).bin

##
## Define the target that creates the firmware volume object file.
##

FWVOL_IMAGES = rtbase runtime/biosrt.elf

$(BINROOT)/biosfwv.o: $(FWVOL_IMAGES)
	@sh $(SRCDIR)/genfwvol.sh $(OBJCOPY)

$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@

runtime/biosrt.elf: runtime


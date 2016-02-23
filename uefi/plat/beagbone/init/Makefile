################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       BeagleBone Stage 1 Loader
#
#   Abstract:
#
#       This module implements Texas Instruments AM335x first stage loader.
#
#   Author:
#
#       Evan Green 17-Dec-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY := bbonemlo.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

INCLUDES += $(SRCROOT)/os/uefi/include;$(SRCROOT)/os/lib/fatlib;
INCLUDES += $(SRCDIR)/../../panda/init;

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

OBJS += boot.o      \
        clock.o     \
        crc32.o     \
        fatboot.o   \
        mux.o       \
        power.o     \
        serial.o    \
        rommem.o    \

ARMV7_OBJS = armv7/start.o  \

TEXT_ADDRESS = 0x402F0408

LINKER_SCRIPT = $(SRCROOT)/os/uefi/plat/panda/init/link.x

EXTRA_SRC_DIRS = armv7

DIRS = bbonefwb \

include $(SRCROOT)/os/minoca.mk

VPATH += $(SRCDIR)/../../panda/init:

postbuild: bbonemlo bbonemlo.bin

bbonemlo: bbonemlo.bin
	@echo Generating - $@
	@bbonefwb/bbonefwb 0x402F0408 $^ $@
	@cp -p $@ $(BINROOT)/

bbonemlo.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@


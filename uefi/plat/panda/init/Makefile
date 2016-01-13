################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       PandaBoard Stage 1 Loader
#
#   Abstract:
#
#       This module implements OMAP4 first stage loader.
#
#   Author:
#
#       Evan Green 1-Apr-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY := omap4mlo.elf

BINARYTYPE = staticapp

BINPLACE = bin

VPATH += $(BINROOT):

INCLUDES += $(SRCROOT)/os/uefi/inc;$(SRCROOT)/os/lib/fatlib;

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

OBJS += boot.o      \
        clock.o     \
        crc32.o     \
        fatboot.o   \
        gpio.o      \
        id.o        \
        mux.o       \
        serial.o    \
        rommem.o    \
        romusb.o    \

ARMV7_OBJS = armv7/start.o  \

TEXT_ADDRESS = 0x40308000

LINKER_SCRIPT = $(SRCROOT)/os/uefi/plat/panda/init/link.x

EXTRA_SRC_DIRS = armv7

DIRS = fwbuild     \

include $(SRCROOT)/os/minoca.mk

postbuild: omap4mlo omap4mlo.bin

omap4mlo: omap4mlo.bin
	@cd $(BINROOT) && pandafwb 0x40300000 $^ $@

omap4mlo.bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


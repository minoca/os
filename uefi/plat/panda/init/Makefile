################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

INCLUDES += $(SRCROOT)/os/uefi/include;

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib -Wl,-zmax-page-size=1

##
## The first stage loader has to be built in ARM mode since the ROM code
## seems to reset if interrupts are enabled and the processor is in Thumb mode.
##

EXTRA_CFLAGS += -marm

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

TEXT_ADDRESS := 0x40308000

EXTRA_SRC_DIRS = armv7

DIRS = fwbuild     \

include $(SRCROOT)/os/minoca.mk

postbuild: omap4mlo

omap4mlo: omap4mlo.bin
	@echo Generating - $@
	@fwbuild/pandafwb 0x40300000 $^ $@
	@cp -p $@ $(BINROOT)/

omap4mlo.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@
	@cp -p $@ $(BINROOT)/


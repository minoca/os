################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

INCLUDES += $(SRCROOT)/os/uefi/include;
INCLUDES += $(SRCDIR)/../../panda/init;

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib -Wl,-zmax-page-size=1

OBJS += boot.o      \
        clock.o     \
        crc32.o     \
        fatboot.o   \
        mux.o       \
        power.o     \
        serial.o    \
        rommem.o    \

ARMV7_OBJS = armv7/start.o  \

TEXT_ADDRESS := 0x402F0408

EXTRA_SRC_DIRS = armv7

DIRS = bbonefwb \

include $(SRCROOT)/os/minoca.mk

VPATH += $(SRCDIR)/../../panda/init:

postbuild: bbonemlo bbonemlo.bin

bbonemlo: bbonemlo.bin
	@echo Generating - $@
	@bbonefwb/bbonefwb $(TEXT_ADDRESS) $^ $@
	@cp -p $@ $(BINROOT)/

bbonemlo.bin: $(BINARY)
	@echo Flattening - $(notdir $<)
	@$(OBJCOPY) -O binary $^ $@


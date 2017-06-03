################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       MBR
#
#   Abstract:
#
#       This module implements the Master Boot Record that is installed at
#       sector 0 of PC/AT disks.
#
#   Author:
#
#       Evan Green 4-Feb-2014
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = mbr

BINARYTYPE = staticapp

BINPLACE = bin

X86_OBJS = mbr.o

X64_OBJS := $(X86_OBJS)

TEXT_ADDRESS = 0x0600

LDFLAGS += -Wl,-zmax-page-size=1 -nodefaultlibs -nostartfiles -nostdlib

include $(SRCROOT)/os/minoca.mk

postbuild: $(BINROOT)/$(BINARY).bin

$(BINROOT)/$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


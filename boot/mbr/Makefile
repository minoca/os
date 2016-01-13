################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
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

TEXT_ADDRESS = 0x0600

LDFLAGS += -Wl,-zmax-page-size=1 -nodefaultlibs -nostartfiles -nostdlib

include $(SRCROOT)/os/minoca.mk

postbuild: $(BINROOT)/$(BINARY).bin

$(BINROOT)/$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


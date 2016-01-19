################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       FATBoot
#
#   Abstract:
#
#       This module implements the boot code between the MBR and the loader on
#       BIOS systems. It knows just enough to load the Boot Manager. It runs
#       in a very gray environment, is undebuggable, and should be considered
#       sacred code.
#
#   Author:
#
#       Evan Green 14-Oct-2013
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = fatboot

BINARYTYPE = staticapp

BINPLACE = bin

INCLUDES += $(SRCDIR)/../lib/inc;$(SRCDIR)/../lib/pcat;$(SRCROOT)/os/lib/fatlib;

X86_OBJS = vbr.o                    \
           fatboot.o                \
           prochw.o                 \
           archsup.o                \
           realmode.o               \
           realmexe.o               \

TEXT_ADDRESS = 0x7C00

LDFLAGS += -Wl,-zmax-page-size=1 -nodefaultlibs -nostartfiles -nostdlib

EXTRA_SRC_DIRS = x86 fw/pcat

include $(SRCROOT)/os/minoca.mk

VPATH += $(SRCDIR)/../lib/x86:$(SRCDIR)/../lib/pcat:

postbuild: $(BINROOT)/$(BINARY).bin

$(BINROOT)/$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


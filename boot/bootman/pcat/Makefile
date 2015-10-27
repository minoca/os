################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Boot Manager (PC/AT)
#
#   Abstract:
#
#       This module implements the Boot Manager that can load a Minoca OS
#       loader. It is targeted for BIOS (non-UEFI) platforms.
#
#   Author:
#
#       Evan Green 21-Feb-2014
#
#   Environment:
#
#       Boot
#
################################################################################

include $(SRCDIR)/../sources

VPATH += $(SRCDIR)/..:

OBJS += bootxfr.o   \
        main.o      \

X86_OBJS = x86/entry.o  \

TEXT_ADDRESS = 0x100000

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

INCLUDES += $(SRCROOT)/os/boot/lib/inc;$(SRCDIR)/..;

TARGETLIBS += $(OBJROOT)/os/boot/lib/pcat/bootpcat.a \
              $(OBJROOT)/os/lib/partlib/partlib.a    \

EXTRA_SRC_DIRS = x86

include $(SRCROOT)/os/minoca.mk

postbuild: $(BINROOT)/$(BINARY).bin

$(BINROOT)/$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


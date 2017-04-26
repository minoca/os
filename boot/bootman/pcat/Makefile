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

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

TEXT_ADDRESS := 0x100000

INCLUDES += $(SRCROOT)/os/boot/lib/include;$(SRCDIR)/..;

TARGETLIBS += $(OBJROOT)/os/boot/lib/pcat/bootpcat.a \
              $(OBJROOT)/os/lib/partlib/partlib.a    \

EXTRA_SRC_DIRS = x86

include $(SRCROOT)/os/minoca.mk

postbuild: $(BINROOT)/$(BINARY).bin

$(BINROOT)/$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


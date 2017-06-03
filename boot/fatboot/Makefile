################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

INCLUDES += $(SRCDIR)/../lib/include; \
            $(SRCDIR)/../lib/pcat;    \

X86_OBJS = vbr.o                    \
           fatboot.o                \
           prochw.o                 \
           archsup.o                \
           realmode.o               \
           x86/realmexe.o           \

X64_OBJS := $(X86_OBJS)

LDFLAGS += -Wl,-zmax-page-size=1 -nodefaultlibs -nostartfiles -nostdlib

##
## This application is always compiled 32-bit, even with a 64-bit target.
##

ifeq ($(ARCH),x64)
EXTRA_CPPFLAGS += -m32
EXTRA_LDFLAGS += -m32
endif

TEXT_ADDRESS := 0x7C00

EXTRA_SRC_DIRS = x86 fw/pcat

include $(SRCROOT)/os/minoca.mk

VPATH += $(SRCDIR)/../lib/x86:$(SRCDIR)/../lib/pcat:

postbuild: $(BINROOT)/$(BINARY).bin

$(BINROOT)/$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


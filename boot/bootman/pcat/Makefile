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
        paging.o    \

X86_OBJS = x86/entry.o  \
           x86/xferc.o  \

X64_OBJS = x86/entry.o  \
           x64/xfera.o  \
           x64/xferc.o  \

LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib

##
## The PCAT boot manager is always compiled for 32-bits, even for AMD64.
##

ifeq ($(ARCH),x64)
EXTRA_CPPFLAGS += -m32
EXTRA_LDFLAGS += -m32
endif

TEXT_ADDRESS := 0x100000

INCLUDES += $(SRCROOT)/os/boot/lib/include;$(SRCDIR)/..;

ifeq ($(ARCH),x64)

TARGETLIBS += $(OBJROOT)/os/boot/lib/pcat/x6432/bootpcat.a \
              $(OBJROOT)/os/lib/partlib/x6432/partlib.a    \

else

TARGETLIBS += $(OBJROOT)/os/boot/lib/pcat/bootpcat.a \
              $(OBJROOT)/os/lib/partlib/partlib.a    \

endif

EXTRA_SRC_DIRS = x86 x64

include $(SRCROOT)/os/minoca.mk

postbuild: $(BINROOT)/$(BINARY).bin

$(BINROOT)/$(BINARY).bin: $(BINARY)
	@cd $(BINROOT) && $(OBJCOPY) -O binary $^ $@


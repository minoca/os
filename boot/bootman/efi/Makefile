################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Boot Manager (UEFI)
#
#   Abstract:
#
#       This module implements the UEFI boot manager that can load a Minoca OS
#       loader.
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

BINARY := bootmefi.elf

VPATH += $(SRCDIR)/..:

OBJS += bootxfr.o   \
        main.o      \

INCLUDES += $(SRCROOT)/os/boot/lib/include;$(SRCDIR)/..;

TARGETLIBS += $(OBJROOT)/os/boot/lib/efi/bootefi.a \

ENTRY = BmEfiApplicationMain

EXTRA_LDFLAGS += -pie -nodefaultlibs -nostartfiles -Wl,--whole-archive

ifeq ($(ARCH), x86)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/include/link_x86.x
endif

ifeq ($(ARCH), armv7)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/include/link_arm.x
TARGETLIBS += $(OBJROOT)/os/kernel/$(ARCH)/boot/armboot.a
LDFLAGS += -Wl,--no-wchar-size-warning
endif

ifeq ($(ARCH), armv6)
LINKER_SCRIPT = $(SRCROOT)/os/uefi/include/link_arm.x
TARGETLIBS += $(OBJROOT)/os/kernel/$(ARCH)/boot/armboot.a
LDFLAGS += -Wl,--no-wchar-size-warning
endif

##
## On architectures that produce ELF binaries, convert it to a PE image.
##

postbuild: bootmefi.efi

bootmefi.efi: $(BINARY)
	@echo Converting - $^
	@cd $(OBJROOT)/$(THISDIR) && elfconv -o $@ -t efiapp $^
	@cd $(OBJROOT)/$(THISDIR) && cp $@ $(BINROOT)

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar


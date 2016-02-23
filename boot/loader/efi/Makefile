################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       OS Loader (UEFI)
#
#   Abstract:
#
#       This module implements the UEFI operating system loader that loads the
#       kernel.
#
#   Author:
#
#       Evan Green 10-Feb-2014
#
#   Environment:
#
#       Boot
#
################################################################################

include $(SRCDIR)/../sources

BINARY := loadefi

VPATH += $(SRCDIR)/..:

OBJS += memory.o    \

INCLUDES += $(SRCROOT)/os/boot/lib/include;$(SRCDIR)/..;

TARGETLIBS += $(OBJROOT)/os/boot/lib/efi/bootefi.a \

ENTRY = BoMain

ifneq ($(ARCH), x86)
LDFLAGS += -Wl,--no-wchar-size-warning
endif

LDFLAGS += -pie

include $(SRCROOT)/os/minoca.mk


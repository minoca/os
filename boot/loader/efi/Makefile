################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

ifeq (,$(filter x86 x64,$(ARCH)))
LDFLAGS += -Wl,--no-wchar-size-warning
endif

LDFLAGS += -pie

include $(SRCROOT)/os/minoca.mk


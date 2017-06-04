################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       PC/AT Boot Library (32-bit)
#
#   Abstract:
#
#       This library contains the PC/AT (BIOS) Boot Library.
#
#   Author:
#
#       Evan Green 2-Jun-2017
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = bootpcat.a

BINARYTYPE = klibrary

include $(SRCDIR)/../../sources

##
## Include the Ke build root to get version.h.
##

INCLUDES += $(SRCDIR)/../../include;$(SRCDIR)/../..;$(OBJROOT)/os/kernel/ke;

OBJS += $(PCAT_OBJS)

X86_OBJS += $(PCAT_X86_OBJS)

X64_OBJS := $(X86_OBJS)

VPATH += $(SRCDIR)/..:

EXTRA_CPPFLAGS += -m32

EXTRA_SRC_DIRS = x86

include $(SRCROOT)/os/minoca.mk


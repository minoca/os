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
#       PC/AT Boot Library
#
#   Abstract:
#
#       This library contains the PC/AT (BIOS) Boot Library.
#
#   Author:
#
#       Evan Green 19-Feb-2014
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = bootpcat.a

BINARYTYPE = klibrary

include $(SRCDIR)/../sources

##
## Include the Ke build root to get version.h.
##

INCLUDES += $(SRCDIR)/../include;$(SRCDIR)/..;$(OBJROOT)/os/kernel/ke;

OBJS += $(PCAT_OBJS)

X86_OBJS += $(PCAT_X86_OBJS)

X64_OBJS += $(PCAT_X64_OBJS)

ifeq ($(ARCH),x64)
DIRS += x6432
endif

EXTRA_SRC_DIRS = x86 x64

include $(SRCROOT)/os/minoca.mk


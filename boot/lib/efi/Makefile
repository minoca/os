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
#       UEFI Boot Library
#
#   Abstract:
#
#       This library contains the UEFI boot library.
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

BINARY = bootefi.a

BINARYTYPE = klibrary

include $(SRCDIR)/../sources

##
## Include the Ke build root to get version.h.
##

INCLUDES += $(SRCDIR)/../include;$(SRCDIR)/..;$(OBJROOT)/os/kernel/ke;

OBJS += dbgser.o   \
        disk.o     \
        fwapi.o    \
        memory.o   \
        time.o     \
        util.o     \
        video.o    \

X86_OBJS += x86/efia.o      \
            x86/efiarch.o

ARMV7_OBJS += armv7/efia.o      \
              armv7/efiarch.o   \

ARMV6_OBJS += armv7/efia.o      \
              armv7/efiarch.o   \

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk

EXTRA_CFLAGS += -fshort-wchar


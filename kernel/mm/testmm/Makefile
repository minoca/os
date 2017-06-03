################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       MM Test
#
#   Abstract:
#
#       This program compiles the kernel memory manager into a user mode
#       application for the purposes of testing it.
#
#   Author:
#
#       Evan Green 27-Jul-2012
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = testmm

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a    \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a  \

VPATH += $(SRCDIR)/..:

OBJS = stubs.o    \
       testmm.o   \
       testmdl.o  \
       testuva.o  \
       block.o    \
       imgsec.o   \
       init.o     \
       invipi.o   \
       iobuf.o    \
       load.o     \
       mdl.o      \
       paging.o   \
       physical.o \
       kpools.o   \
       virtual.o  \
       fault.o    \

ARMV7_OBJS = armv7/mapping.o  \

ARMV6_OBJS = armv7/mapping.o  \

X86_OBJS = x86/mapping.o  \

##
## TODO: Use the x64 mapping.o once mm is implemented.
##

X64_OBJS = x86/mapping.o

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk


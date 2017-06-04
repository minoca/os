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
#       ARMv7
#
#   Abstract:
#
#       This library implements support routines for the ARMv7 architecture.
#
#   Author:
#
#       Evan Green 11-Aug-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = armsup.a

BINARYTYPE = klibrary

ARMV7_OBJS = archsup.o  \
             archsupc.o \
             commsup.o  \
             inttable.o \
             prochw.o   \
             sstep.o    \
             trap.o     \
             vfp.o      \

DIRS = boot

boot: $(BINARY)

include $(SRCROOT)/os/minoca.mk

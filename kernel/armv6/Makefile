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
#       ARMv6
#
#   Abstract:
#
#       This library implements support routines for the ARMv6 architecture.
#
#   Author:
#
#       Chris Stevens 2-Feb-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = armsup.a

BINARYTYPE = klibrary

ARMV6_OBJS = archsup.o  \
             archsupc.o \
             ../armv7/commsup.o  \
             ../armv7/inttable.o \
             ../armv7/prochw.o   \
             ../armv7/sstep.o    \
             ../armv7/trap.o     \
             ../armv7/vfp.o      \

DIRS = boot

boot: $(BINARY)

include $(SRCROOT)/os/minoca.mk


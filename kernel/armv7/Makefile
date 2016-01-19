################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

BINARYTYPE = library

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

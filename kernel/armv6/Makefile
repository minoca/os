################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
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

BINARYTYPE = library

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


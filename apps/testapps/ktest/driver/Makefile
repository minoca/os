################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Kernel Test Driver
#
#   Abstract:
#
#       This module implements the kernel driver portion of the kernel test.
#
#   Author:
#
#       Evan Green 5-Nov-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = ktestdrv.drv

BINARYTYPE = so

BINPLACE = bin

INCLUDES += $(SRCDIR)/..;

OBJS = ktestdrv.o    \
       tblock.o      \
       tdesc.o       \
       testsup.o     \
       tpool.o       \
       tthread.o     \
       ttpc.o        \
       twork.o       \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk


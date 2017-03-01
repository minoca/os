################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = driver

BINPLACE = bin

INCLUDES += $(SRCDIR)/..;

OBJS = ktestdrv.o    \
       tblock.o      \
       tdesc.o       \
       testsup.o     \
       tpool.o       \
       tthread.o     \
       twork.o       \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk


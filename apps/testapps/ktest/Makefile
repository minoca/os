################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Kernel Test
#
#   Abstract:
#
#       This executable implements the kernel test application, which loads a
#       driver, executes kernel mode stress tests, and reports the results back
#       to user mode.
#
#   Author:
#
#       Evan Green 5-Nov-2013
#
#   Environment:
#
#       User
#
################################################################################

BINARY = ktest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = ktest.o \

DYNLIBS = -lminocaos

DIRS = driver

include $(SRCROOT)/os/minoca.mk


################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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


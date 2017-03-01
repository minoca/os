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
#       RAM Disk
#
#   Abstract:
#
#       This module implements a simple RAM disk driver.
#
#   Author:
#
#       Evan Green 17-Oct-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ramdisk.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = ramdisk.o

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk


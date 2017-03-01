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
#       FAT
#
#   Abstract:
#
#       This module implements the File Allocation Table file system driver.
#       It supports FAT12, FAT16, and FAT32 with some long file name support.
#
#   Author:
#
#       Evan Green 25-Sep-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = fat.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = fatfs.o    \
       fatio.o    \

TARGETLIBS = $(OBJROOT)/os/lib/fatlib/fat.a    \

DYNLIBS = $(BINROOT)/kernel                \

include $(SRCROOT)/os/minoca.mk


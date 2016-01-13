################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

BINARYTYPE = so

BINPLACE = bin

OBJS = fatfs.o    \
       fatio.o    \

TARGETLIBS = $(INTLIB)/os/lib/fatlib/fat.a    \

DYNLIBS = $(BINROOT)/kernel                \

include $(SRCROOT)/os/minoca.mk


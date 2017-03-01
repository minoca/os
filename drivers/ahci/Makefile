################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       AHCI
#
#   Abstract:
#
#       This module implements the driver for the AHCI (Advanced Host
#       Controller Interface) storage host.
#
#   Author:
#
#       Evan Green 15-Nov-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ahci.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = ahci.o   \
       ahcihw.o \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk


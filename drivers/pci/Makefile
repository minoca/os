################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       PCI
#
#   Abstract:
#
#       This module implements the Peripheral Component Interconnect (PCI)
#       driver.
#
#   Author:
#
#       Evan Green 16-Sep-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = pci.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = msi.o     \
       pci.o     \
       rootbus.o \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk


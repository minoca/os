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

BINARYTYPE = driver

BINPLACE = bin

OBJS = msi.o     \
       pci.o     \
       rootbus.o \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk


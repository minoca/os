################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       TI AM335x I2C Driver
#
#   Abstract:
#
#       This module implements the I2C controller driver for the TI AM335x SoC.
#
#   Author:
#
#       Evan Green 7-Sep-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = am3i2c.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = am3i2c.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/spb.drv             \

include $(SRCROOT)/os/minoca.mk


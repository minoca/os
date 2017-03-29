################################################################################
#
#   Copyright (c) 2017 Minoca Corp. All Rights Reserved
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Broadcom 27xx I2C Driver
#
#   Abstract:
#
#       This module implements the I2C controller driver for the Broadcom 27xx
#       SoC.
#
#   Author:
#
#       Chris Stevens 18-Jan-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = bcm27i2c.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = bcm27i2c.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/spb.drv             \

include $(SRCROOT)/os/minoca.mk


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
#       OMAP4 GPIO
#
#   Abstract:
#
#       This module implements General Purpose I/O support for the TI OMAP4
#       SoC in the kernel.
#
#   Author:
#
#       Evan Green 4-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = om4gpio.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = om4gpio.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/gpio.drv            \

include $(SRCROOT)/os/minoca.mk


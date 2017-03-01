################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       BCM27xx GPIO
#
#   Abstract:
#
#       This module implements General Purpose I/O support for the Broadcom
#       27xx SoCs.
#
#   Author:
#
#       Chris Stevens 10-May-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = bc27gpio.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = bc27gpio.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/gpio.drv            \

include $(SRCROOT)/os/minoca.mk


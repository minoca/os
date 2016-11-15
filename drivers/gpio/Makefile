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
#       GPIO
#
#   Abstract:
#
#       This directory contains drivers related to General Purpose Input/Output.
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

GPIO_DRIVERS = broadcom \
               rockchip \
               ti       \

DIRS = core             \
       $(GPIO_DRIVERS)  \

include $(SRCROOT)/os/minoca.mk

$(GPIO_DRIVERS): core


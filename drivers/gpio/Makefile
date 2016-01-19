################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
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

GPIO_DRIVERS = rockchip \
               ti       \

DIRS = core             \
       $(GPIO_DRIVERS)  \

include $(SRCROOT)/os/minoca.mk

$(GPIO_DRIVERS): core


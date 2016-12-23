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
#       DMA
#
#   Abstract:
#
#       This directory contains system DMA drivers.
#
#   Author:
#
#       Evan Green 1-Feb-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

DMA_DRIVERS = bcm2709  \
              edma3    \

DIRS = core             \
       $(DMA_DRIVERS)   \

include $(SRCROOT)/os/minoca.mk

$(DMA_DRIVERS): core


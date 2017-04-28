################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Input
#
#   Abstract:
#
#       This directory contains User Input related drivers.
#
#   Author:
#
#       Evan Green 28-Apr-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

INPUT_DEVICE_DRIVERS := i8042 \

DIRS = usrinput                \
       $(INPUT_DEVICE_DRIVERS) \

include $(SRCROOT)/os/minoca.mk

$(INPUT_DEVICE_DRIVERS): usrinput


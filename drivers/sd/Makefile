################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All rights reserved.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       SD
#
#   Abstract:
#
#       This file is responsible for building the Secure Digital host controller
#       driver.
#
#   Author:
#
#       Evan Green 16-Mar-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

SD_CONTROLLERS = bcm2709    \
                 omap4      \
                 rk32xx     \

DIRS = core                 \
       $(SD_CONTROLLERS)    \

include $(SRCROOT)/os/minoca.mk

$(SD_CONTROLLERS): core


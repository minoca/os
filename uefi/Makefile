################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       UEFI
#
#   Abstract:
#
#       This directory builds UEFI firmware images for several platforms.
#
#   Author:
#
#       Evan Green 26-Feb-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

DIRS = archlib  \
       core     \
       dev      \
       plat     \
       tools    \

include $(SRCROOT)/os/minoca.mk

plat: core dev tools
core: archlib tools


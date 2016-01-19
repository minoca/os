################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
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


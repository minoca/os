################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Platform Support
#
#   Abstract:
#
#       This directory contains platform-specific targets for UEFI firmware.
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

ifeq ($(ARCH), x86)

DIRS += bios    \

endif

ifeq ($(ARCH), armv7)

DIRS += beagbone \
        integcp  \
        panda    \
        rpi2     \
        veyron   \

endif

ifeq ($(ARCH), armv6)

DIRS += rpi     \

endif

include $(SRCROOT)/os/minoca.mk

beagbone: panda


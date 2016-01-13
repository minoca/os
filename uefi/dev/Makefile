################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Devices
#
#   Abstract:
#
#       This directory builds UEFI device support libraries.
#
#   Author:
#
#       Evan Green 27-Feb-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

ifeq ($(ARCH), x86)

DIRS += ns16550     \

endif

ifeq ($(ARCH), armv7)

DIRS += bcm2709     \
        gic         \
        ns16550     \
        omap4       \
        omapuart    \
        pl031       \
        pl11        \
        pl110       \
        sd          \

endif

ifeq ($(ARCH), armv6)

DIRS += bcm2709     \
        pl11        \
        sd          \

endif

include $(SRCROOT)/os/minoca.mk


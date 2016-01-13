################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Boot Library
#
#   Abstract:
#
#       This directory builds all flavors of the boot library.
#
#   Author:
#
#       Evan Green 19-Feb-2014
#
#   Environment:
#
#       Boot
#
################################################################################

DIRS = efi

ifeq ($(ARCH), x86)

DIRS += pcat

endif

EXTRA_SRC_DIRS = x86 armv7 armv6

include $(SRCROOT)/os/minoca.mk

pcat: efi


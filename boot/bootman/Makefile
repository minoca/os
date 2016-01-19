################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Boot Manager
#
#   Abstract:
#
#       This module implements the boot manager, which can load an operating
#       system loader. In a multi-boot system, there would be one boot manager
#       that can load one of many OS loaders (including downstream loaders).
#
#   Author:
#
#       Evan Green 21-Feb-2014
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

include $(SRCROOT)/os/minoca.mk


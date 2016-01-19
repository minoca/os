################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Boot
#
#   Abstract:
#
#       This module implements support for the boot environment, which contains
#       the applications and support code needed to load and launch the
#       operating system kernel. It consists of some assembly bootstrap code,
#       a boot manager, and an OS loader.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Boot
#
################################################################################

DIRS = bootman  \
       lib      \
       loader   \

ifeq ($(ARCH), x86)

DIRS += fatboot \
        mbr     \

endif

include $(SRCROOT)/os/minoca.mk

loader bootman: lib
fatboot: lib


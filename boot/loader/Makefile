################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       OS Loader
#
#   Abstract:
#
#       This module implements the operating system loader that loads and
#       launches the kernel. It is loaded by the Boot Manager, and is usually
#       tied to a specific kernel version.
#
#   Author:
#
#       Evan Green 29-Jul-2012
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


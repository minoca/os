################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

ifneq (,$(filter x86 x64,$(ARCH)))

DIRS += pcat

endif

include $(SRCROOT)/os/minoca.mk


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


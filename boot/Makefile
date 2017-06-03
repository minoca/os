################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

ifneq (,$(filter x86 x64,$(ARCH)))

DIRS += fatboot \
        mbr     \

endif

include $(SRCROOT)/os/minoca.mk

loader bootman: lib
fatboot: lib


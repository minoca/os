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

ifneq (,$(filter x86 x64,$(ARCH)))

DIRS += pcat

endif

include $(SRCROOT)/os/minoca.mk


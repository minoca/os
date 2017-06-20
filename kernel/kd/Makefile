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
#       Kernel Debugger
#
#   Abstract:
#
#       This library contains the support code necessary to enable debugging
#       of a running kernel. This library is included in all versions of the
#       operating system.
#
#   Author:
#
#       Evan Green 10-Aug-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = kd.a

BINARYTYPE = klibrary

include $(SRCDIR)/sources

ARMV7_OBJS += armv7/kdatomic.o \

EXTRA_SRC_DIRS = x86 x64 armv7 armv6

DIRS = boot kdusb

include $(SRCROOT)/os/minoca.mk


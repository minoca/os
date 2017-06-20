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
#       Kernel Debugger (Boot)
#
#   Abstract:
#
#       This library builds a portion of the Kernel Debugger Library for use in
#       the boot environment.
#
#   Author:
#
#       Chris Stevens 18-Mar-2014
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = kdboot.a

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

ARMV7_OBJS += boot/armv7/kdatomic.o \

EXTRA_SRC_DIRS = x86 x64 armv7 armv6 boot/armv7

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk


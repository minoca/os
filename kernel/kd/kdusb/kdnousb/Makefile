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
#       KD No-USB
#
#   Abstract:
#
#       This library contains stubs for the KD USB library, for those who don't
#       need USB debug support.
#
#   Author:
#
#       Evan Green 18-Apr-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = kdnousb.a

BINARYTYPE = klibrary

OBJS = stubs.o   \

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk


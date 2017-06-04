################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       KD No-USB (32-bit)
#
#   Abstract:
#
#       This library contains stubs for the KD USB library, for those who don't
#       need USB debug support.
#
#   Author:
#
#       Evan Green 2-Jun-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = kdnousb.a

BINARYTYPE = klibrary

OBJS = stubs.o   \

VPATH += $(SRCDIR)/..:

EXTRA_CPPFLAGS += -m32

include $(SRCROOT)/os/minoca.mk


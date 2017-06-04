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
#       Memory Manager (32-bit Boot)
#
#   Abstract:
#
#       This library builds a subset of the Memory Manager library for use in
#       the boot environment.
#
#   Author:
#
#       Evan Green 2-Jun-2017
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = mmboot.a

BINARYTYPE = klibrary

OBJS = mdl.o          \

VPATH += $(SRCDIR)/../..:

EXTRA_CPPFLAGS += -m32

include $(SRCROOT)/os/minoca.mk


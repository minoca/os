################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Base Runtime Library (Wide)
#
#   Abstract:
#
#       This library contains the wide character support supplement to the base
#       runtime library.
#
#   Author:
#
#       Evan Green 31-Aug-2015
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = basertlw.a

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS := $(OBJS_WIDE)

ARMV7_OBJS :=

ARMV6_OBJS :=

X86_OBJS :=

X64_OBJS :=

include $(SRCROOT)/os/minoca.mk


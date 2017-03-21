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
#       DbgExt (Build Machine)
#
#   Abstract:
#
#       This module implements the import library for kernel debugger
#       extensions on the build machine.
#
#   Author:
#
#       Evan Green 20-Mar-2017
#
#   Environment:
#
#       Debug Client
#
################################################################################

BINARY = dbgext.a

BINARYTYPE = library

BINPLACE = tools/lib

BUILD = yes

OBJS = extimp.o   \

VPATH += $(SRCDIR)/..:

include $(SRCROOT)/os/minoca.mk


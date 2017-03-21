################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       DbgExtNt
#
#   Abstract:
#
#       This module implements the import library for kernel debugger
#       extensions on a Windows host.
#
#   Author:
#
#       Evan Green 8-May-2013
#
#   Environment:
#
#       Debug Client
#
################################################################################

BINARY = dbgextnt.a

BINARYTYPE = library

BINPLACE = tools/lib

VPATH += $(SRCDIR)/..:

BUILD = yes

OBJS = extimp.o   \

include $(SRCROOT)/os/minoca.mk

##
## Override the compiler since this is a Windows library.
##

CC := gcc
LD := ld
RCC := windres
AR := ar
AS := as


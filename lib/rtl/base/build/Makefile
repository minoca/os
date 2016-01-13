################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Base Runtime Library (Build)
#
#   Abstract:
#
#       This library contains the base Runtime Library that is shared between
#       kernel and user modes, compiled to target the build machine.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = basertl.a

BINARYTYPE = library

BUILD = yes

BINPLACE = intlib

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

include ../sources

EXTRA_SRC_DIRS = x86 x64 armv7 armv6

include $(SRCROOT)/os/minoca.mk


################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Base Runtime Library
#
#   Abstract:
#
#       This library contains the base Runtime Library that is shared between
#       kernel and user modes.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = basertl.a

BINARYTYPE = library

BINPLACE = intlib

INCLUDES += $(SRCDIR)/..;

include $(SRCDIR)/sources

EXTRA_SRC_DIRS = x86 x64 armv7

DIRS = boot     \
       build    \
       intrins  \
       wide     \

include $(SRCROOT)/os/minoca.mk


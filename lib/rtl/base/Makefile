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

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/..;

include $(SRCDIR)/sources

EXTRA_SRC_DIRS = x86 x64 armv7

DIRS = boot     \
       build    \
       intrins  \
       wide     \

include $(SRCROOT)/os/minoca.mk


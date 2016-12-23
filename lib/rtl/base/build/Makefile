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

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS := $(OBJS)      \
        $(OBJS_WIDE) \

##
## Make sure softfp is in there. On an ARM build system it would be.
##

ifeq (, $(filter fp2int.o,$(OBJS)))
OBJS += fp2int.o \
        softfp.o

endif

EXTRA_SRC_DIRS = x86 x64 armv7

include $(SRCROOT)/os/minoca.mk


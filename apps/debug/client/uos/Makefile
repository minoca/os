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
#       Build Debugger
#
#   Abstract:
#
#       This program implements the Minoca debugger compiled for the build
#       machine.
#
#   Author:
#
#       Evan Green 20-Mar-2017
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = debug

BINPLACE = tools/bin

BINARYTYPE = app

BUILD = yes

INCLUDES += $(SRCDIR)/..;                                 \
            $(SRCROOT)/os/lib/im;                         \

TARGETLIBS = $(OBJROOT)/os/lib/im/build/im.a              \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a   \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a      \

ifneq ($(OS),$(filter Windows_NT Minoca FreeBSD,$(OS)))

DYNLIBS = -ldl -pthread

endif

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       $(UOS_OBJS)      \

include $(SRCROOT)/os/minoca.mk


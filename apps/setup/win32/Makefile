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
#       msetup (Win32)
#
#   Abstract:
#
#       This executable implements the setup (OS installer) executable running
#       on a Windows host.
#
#   Author:
#
#       Evan Green 8-Oct-2014
#
#   Environment:
#
#       User
#
################################################################################

BINARY = msetup

BINPLACE = tools/bin

BUILD = yes

BINARYTYPE = build

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       io.o             \
       misc.o           \
       msetuprc.rsc     \
       part.o           \
       win32sup.o       \

TARGETLIBS = $(OBJROOT)/os/lib/partlib/build/partlib.a         \
             $(OBJROOT)/os/lib/fatlib/build/fat.a              \
             $(OBJROOT)/os/lib/bconflib/build/bconflib.a       \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a        \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a      \
             $(OBJROOT)/os/lib/yy/build/yy.a                   \
             $(OBJROOT)/os/apps/ck/lib/build/libchalk.a        \

DYNLIBS = -lsetupapi

include $(SRCROOT)/os/minoca.mk


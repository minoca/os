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
#       Win32 Debugger (GUI)
#
#   Abstract:
#
#       This module implements the graphical version of the debugger on Win32
#       platforms.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Win32
#
################################################################################

BINARY = debugui

BINARYTYPE = build

BUILD = yes

BINPLACE = tools/bin

INCLUDES += $(SRCDIR)/../..;
INCLUDES += $(SRCROOT)/os/lib/im;

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a      \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a    \
             $(OBJROOT)/os/lib/im/build/imu.a                \

DYNLIBS = -lpsapi -lws2_32 -lmswsock -ladvapi32 -lshlwapi

EXTRA_LDFLAGS += -mwindows

VPATH += $(SRCDIR)/..:$(SRCDIR)/../..:

include $(SRCDIR)/../../sources

OBJS = $(COMMON_OBJS) \
       debugres.rsc   \
       ntcomm.o       \
       ntdbgui.o      \
       ntextsup.o     \
       ntsock.o       \
       ntusrdbg.o     \
       ntusrsup.o     \

include $(SRCROOT)/os/minoca.mk


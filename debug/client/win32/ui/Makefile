################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

BINARY = debugui.exe

BINARYTYPE = win32

BUILD = yes

BINPLACE = bin

INCLUDES += $(SRCDIR)/../../..;$(SRCDIR)/../..;
INCLUDES += $(SRCROOT)/os/lib/im;

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a      \
             $(OBJROOT)/os/lib/rtl/build/rtlstubs.a          \
             $(OBJROOT)/os/lib/im/build/im.a                 \

DYNLIBS = -lpsapi -lws2_32 -lmswsock -ladvapi32 -lshlwapi

VPATH += $(SRCDIR)/..:$(SRCDIR)/../..:

ifeq ($(ARCH), x86)

include $(SRCDIR)/../../sources

X86_OBJS += $(COMMON_OBJS)   \
            debugres.rsc \
            ntcomm.o     \
            ntdbgui.o    \
            ntextsup.o   \
            ntsock.o     \
            ntusrdbg.o   \
            ntusrsup.o   \

endif

include $(SRCROOT)/os/minoca.mk


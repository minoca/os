################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Win32 Debugger (Command Line)
#
#   Abstract:
#
#       This program implements the command line version of the debugger for
#       Win32 platforms.
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

BINARY = debug.exe

BINARYTYPE = ntconsole

BUILD = yes

BINPLACE = bin

INCLUDES += $(SRCDIR)/../../..;$(SRCDIR)/../..;
INCLUDES += $(SRCROOT)/os/lib/im;

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a      \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a         \
             $(OBJROOT)/os/lib/im/build/im.a                 \

DYNLIBS = -lpsapi -lws2_32 -lmswsock -ladvapi32

VPATH += $(SRCDIR)/..:$(SRCDIR)/../..:

ifeq ($(ARCH), x86)

include $(SRCDIR)/../../sources

X86_OBJS += $(COMMON_OBJS)   \
            ntcmdln.o        \
            ntcomm.o         \
            ntextsup.o       \
            ntsock.o         \
            ntusrdbg.o       \
            ntusrsup.o       \

endif

include $(SRCROOT)/os/minoca.mk


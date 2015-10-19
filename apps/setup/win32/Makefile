################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       setup (Win32)
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

BINARY = msetup.exe

BINPLACE = bin

BINARYTYPE = ntconsole

INCLUDES += $(SRCROOT)/os/apps/inc;             \

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       io.o             \
       misc.o           \
       part.o           \
       win32sup.o       \

TARGETLIBS = $(OBJROOT)/os/lib/partlib/build/partlib.a      \
             $(OBJROOT)/os/lib/fatlib/build/fat.a           \
             $(OBJROOT)/os/lib/bconflib/build/bconflib.a    \
             $(INTLIB)/os/lib/rtl/base/build/basertl.a      \
             $(INTLIB)/os/lib/rtl/build/rtlstubs.a          \
             $(OBJROOT)/os/lib/yy/build/yy.a                \

DYNLIBS = -lsetupapi

include $(SRCROOT)/os/minoca.mk

##
## Override the compiler since this is a Windows app.
##

CC := gcc
LD := ld
RCC := windres
AR := ar
AS := as


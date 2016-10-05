################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

BINARY = msetup.exe

BINPLACE = tools/bin

BUILD = yes

BINARYTYPE = ntconsole

INCLUDES += $(SRCROOT)/os/apps/lib/chalk;       \
            $(SRCDIR)/../config;                \

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
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a           \
             $(OBJROOT)/os/lib/yy/build/yy.a                   \
             $(OBJROOT)/os/apps/lib/chalk/build/chalk.a        \

DYNLIBS = -lsetupapi

include $(SRCROOT)/os/minoca.mk

config.o: $(CONFIG_FILES)

##
## Override the compiler since this is a Windows app.
##

CC := gcc
LD := ld
RCC := windres
AR := ar
AS := as


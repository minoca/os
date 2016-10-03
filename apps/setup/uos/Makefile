################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       msetup (Build)
#
#   Abstract:
#
#       This executable implements the setup (OS installer) executable for
#       POSIX based build machines.
#
#   Author:
#
#       Evan Green 19-Jan-2016
#
#   Environment:
#
#       User
#
################################################################################

BINARY = msetup

BINPLACE = tools/bin

BUILD = yes

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/lib/chalk;       \

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

ifeq ($(OS),Minoca)

OBJS = $(COMMON_OBJS)   \
       minoca/io.o      \
       minoca/misc.o    \
       minoca/part.o    \

DYNLIBS = -lminocaos

else

OBJS = $(COMMON_OBJS)   \
       io.o      \
       misc.o    \
       part.o    \

endif

TARGETLIBS = $(OBJROOT)/os/lib/partlib/build/partlib.a         \
             $(OBJROOT)/os/lib/fatlib/build/fat.a              \
             $(OBJROOT)/os/lib/bconflib/build/bconflib.a       \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a        \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a           \
             $(OBJROOT)/os/apps/setup/config/build/msetplat.a  \
             $(OBJROOT)/os/apps/lib/chalk/build/chalk.a        \
             $(OBJROOT)/os/lib/yy/build/yy.a                   \

EXTRA_SRC_DIRS = ../minoca

include $(SRCROOT)/os/minoca.mk


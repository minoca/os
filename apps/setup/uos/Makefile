################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       msetup_build
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

BINARY = msetup_build

BINPLACE = bin

BUILD = yes

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc;             \
            $(SRCROOT)/os/apps/lib/chalk;       \

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
             $(OBJROOT)/os/lib/yy/build/yy.a                   \
             $(OBJROOT)/os/apps/setup/config/build/msetplat.a  \
             $(OBJROOT)/os/apps/lib/chalk/build/chalk.a        \

include $(SRCROOT)/os/minoca.mk


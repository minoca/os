################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

ifeq ($(shell uname -s),Minoca)

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

ifneq ($(shell uname -s),FreeBSD)
DYNLIBS = -ldl
endif

endif

TARGETLIBS = $(OBJROOT)/os/lib/partlib/build/partlib.a         \
             $(OBJROOT)/os/lib/fatlib/build/fat.a              \
             $(OBJROOT)/os/lib/bconflib/build/bconflib.a       \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a        \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a      \
             $(OBJROOT)/os/apps/ck/lib/build/libchalk.a        \
             $(OBJROOT)/os/lib/yy/build/yy.a                   \

EXTRA_SRC_DIRS = minoca

include $(SRCROOT)/os/minoca.mk


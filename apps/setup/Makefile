################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       msetup
#
#   Abstract:
#
#       This executable implements the setup (OS installer) executable.
#
#   Author:
#
#       Evan Green 10-Apr-2014
#
#   Environment:
#
#       User
#
################################################################################

BINARY = msetup

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc;             \
            $(SRCROOT)/os/apps/inc/libc;        \
            $(SRCROOT)/os/apps/lib/chalk;       \

include $(SRCDIR)/sources

OBJS = $(COMMON_OBJS)   \
       minoca/io.o      \
       minoca/misc.o    \
       minoca/part.o    \

TARGETLIBS = $(OBJROOT)/os/lib/partlib/partlib.a            \
             $(OBJROOT)/os/lib/fatlib/fat.a                 \
             $(OBJROOT)/os/lib/bconflib/bconflib.a          \
             $(OBJROOT)/os/lib/rtl/base/basertl.a           \
             $(OBJROOT)/os/apps/osbase/urtl/urtl.a          \
             $(OBJROOT)/os/apps/lib/chalk/chalk.a           \
             $(OBJROOT)/os/lib/yy/yy.a                      \
             $(OBJROOT)/os/apps/setup/config/msetplat.a     \

DYNLIBS = -lminocaos

DIRS = config

ifeq ($(OS),Windows_NT)

DIRS += win32

else

DIRS += uos

endif

EXTRA_SRC_DIRS = uos

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/bin/$(BINARY); then \
	    $(STRIP) -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY); \
	fi


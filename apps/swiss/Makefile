################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       swiss
#
#   Abstract:
#
#       This executable implements the Swiss Army Knife utility, which contains
#       many basic core utilities, including a Bourne-compatible shell.
#
#   Author:
#
#       Evan Green 5-Jun-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = swiss

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCDIR)/;$(SRCROOT)/os/apps/inc/libc;$(SRCROOT)/os/apps/inc;

include $(SRCDIR)/sources

OBJS = $(COMMON_OBJS)   \
       $(UOS_OBJS)      \
       cmds.o           \
       dw.o             \
       swlib/minocaos.o \

TARGETLIBS = $(OBJROOT)/os/lib/termlib/termlib.a     \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

ifeq ($(OS),Windows_NT)

DIRS = win32

else
ifneq ($(OS),Minoca)
DIRS = uos
endif
endif

EXTRA_LDFLAGS += -L$(BINROOT)
EXTRA_CFLAGS += -ftls-model=initial-exec

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/bin/sh; then \
	    $(STRIP) -o $(BINROOT)/skel/bin/sh $(BINROOT)/$(BINARY); \
	fi


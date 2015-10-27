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
       chown.o          \
       cmds.o           \
       init.o           \
       login/chpasswd.o \
       login/getty.o    \
       login/groupadd.o \
       login/groupdel.o \
       login/login.o    \
       login/lutil.o    \
       login/passwd.o   \
       login/su.o       \
       login/sulogin.o  \
       login/useradd.o  \
       login/userdel.o  \
       login/vlock.o    \
       mkfifo.o         \
       readlink.o       \
       sh/shuos.o       \
       ssdaemon.o       \
       swlib/chownutl.o \
       swlib/minocaos.o \
       swlib/uos.o      \
       telnet.o         \
       telnetd.o        \

TARGETLIBS = $(OBJROOT)/os/lib/termlib/termlib.a     \

DYNLIBS = -lminocaos

ifeq ($(OS),Windows_NT)

DIRS = win32

endif

include $(SRCROOT)/os/minoca.mk

EXTRA_LDFLAGS += -L$(BINROOT)
EXTRA_CFLAGS += -ftls-model=initial-exec

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/bin/sh; then \
	    $(STRIP) -o $(BINROOT)/skel/bin/sh $(BINROOT)/$(BINARY); \
	fi


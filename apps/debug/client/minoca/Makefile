################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Native Debugger
#
#   Abstract:
#
#       This program implements the native debugger (command line version),
#       which runs on Minoca OS.
#
#   Author:
#
#       Evan Green 27-May-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = debug

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCDIR)/..;                        \
            $(SRCROOT)/os/lib/im;                \
            $(SRCROOT)/os/apps/libc/include;

TARGETLIBS = $(OBJROOT)/os/lib/im/im.a                  \

DYNLIBS = -lminocaos

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       cmdln.o          \
       extsup.o         \
       sock.o           \

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/bin/$(BINARY); then \
	    $(STRIP) -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY); \
	fi


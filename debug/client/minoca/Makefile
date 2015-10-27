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

INCLUDES += $(SRCROOT)/os/apps/inc;$(SRCDIR)/..;$(SRCDIR)/../..;
INCLUDES += $(SRCROOT)/os/lib/im;$(SRCROOT)/os/apps/inc/libc;

TARGETLIBS = $(OBJROOT)/os/lib/im/im.a                  \

DYNLIBS = -lminocaos

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       cmdln.o          \
       extsup.o         \
       sock.o           \

include $(SRCROOT)/os/minoca.mk


################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Static pthread
#
#   Abstract:
#
#       This module implements functions that must be statically linked for the
#       POSIX thread library.
#
#   Author:
#
#       Evan Green 4-May-2015
#
#   Environment:
#
#       User Mode C Library
#
################################################################################

BINARY = libpthread_nonshared.a

BINARYTYPE = library

BINPLACE = bin

OBJS = ptatfork.o \

INCLUDES += $(SRCROOT)/os/apps/libc/include;
INCLUDES += $(SRCDIR)/..;$(SRCDIR)/../..;

include $(SRCROOT)/os/minoca.mk


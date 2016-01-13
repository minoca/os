################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Signal Test
#
#   Abstract:
#
#       This executable implements the signal test application.
#
#   Author:
#
#       Evan Green 6-May-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = sigtest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc/libc;$(SRCROOT)/os/apps/inc;

OBJS = sigtest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


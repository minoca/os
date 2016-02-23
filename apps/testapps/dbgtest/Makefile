################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Debug Test
#
#   Abstract:
#
#       This executable implements the debug API test application.
#
#   Author:
#
#       Evan Green 15-May-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = dbgtest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/include/libc;$(SRCROOT)/os/apps/include;

OBJS = dbgtest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


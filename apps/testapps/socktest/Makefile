################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Socket Test
#
#   Abstract:
#
#       This executable implements the socket test application.
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

BINARY = socktest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc/libc;$(SRCROOT)/os/apps/inc;

OBJS = socktest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


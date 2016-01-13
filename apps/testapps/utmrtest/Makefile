################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Timer Test
#
#   Abstract:
#
#       This executable implements the user mode timer test application.
#
#   Author:
#
#       Evan Green 11-Aug-2013
#
#   Environment:
#
#       User
#
################################################################################

BINARY = utmrtest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc/libc;$(SRCROOT)/os/apps/inc;

OBJS = utmrtest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


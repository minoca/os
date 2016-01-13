################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       InitCon
#
#   Abstract:
#
#       This executable implements the console initialization application. It
#       wires up standard input and output to the local video display, and
#       executes the real initialization program.
#
#   Author:
#
#       Evan Green 14-Mar-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = initcon

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/drivers/usrinput/inc; \
            $(SRCROOT)/os/apps/inc;             \
            $(SRCROOT)/os/apps/inc/libc;

OBJS = initcon.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


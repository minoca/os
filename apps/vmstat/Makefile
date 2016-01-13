################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       vmstat
#
#   Abstract:
#
#       This executable implements the vmstat application, which prints current
#       information about kernel memory usage.
#
#   Author:
#
#       Evan Green 5-Mar-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = vmstat

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc;      \
            $(SRCROOT)/os/apps/inc/libc; \

OBJS = vmstat.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


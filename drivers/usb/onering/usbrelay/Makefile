################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       usbrelay
#
#   Abstract:
#
#       This executable implements the usbrelay application, which is a
#       simple application that connects to and communicates with the USB
#       relay board from One Ring Road.
#
#   Author:
#
#       Evan Green 26-Jan-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = usbrelay

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/include;      \
            $(SRCROOT)/os/apps/include/libc; \

OBJS = usbrelay.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


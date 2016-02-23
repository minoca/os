################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       profile
#
#   Abstract:
#
#       This executable implements the profile application. It is used to
#       enable and disable the Minoca System Profiler.
#
#   Author:
#
#       Chris Stevens 18-Jan-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = profile

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/include;      \
            $(SRCROOT)/os/apps/include/libc; \

OBJS = profile.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


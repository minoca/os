################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       mount
#
#   Abstract:
#
#       This executable implements the mount application. It is used to mount
#       block devices, directories, devices, and files onto other directories
#       or files.
#
#   Author:
#
#       Chris Stevens 30-Jul-2013
#
#   Environment:
#
#       User
#
################################################################################

BINARY = mount

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc;      \
            $(SRCROOT)/os/apps/inc/libc; \

OBJS = mount.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk


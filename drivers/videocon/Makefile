################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Video Console
#
#   Abstract:
#
#       This module implements a basic console on a framebuffer.
#
#   Author:
#
#       Evan Green 15-Feb-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = videocon.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = videocon.o    \

DYNLIBS = $(BINROOT)/kernel                 \

TARGETLIBS = $(INTLIB)/os/lib/basevid/basevid.a     \
             $(INTLIB)/os/lib/termlib/termlib.a     \

include $(SRCROOT)/os/minoca.mk


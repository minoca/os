################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       RTL C (Build)
#
#   Abstract:
#
#       This library contains stub functions to allow the Rtl Library to
#       compile in a standard C library on the build machine.
#
#   Author:
#
#       Evan Green 23-Oct-2012
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = rtlc.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

OBJS = stubs.o   \

include $(SRCROOT)/os/minoca.mk


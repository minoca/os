################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Chalk (Shared)
#
#   Abstract:
#
#       This shared library contains the Chalk interpreter, which provides a
#       basic programming language.
#
#   Author:
#
#       Evan Green 9-Jun-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = libchalk.so.1

BINARYTYPE = so

BINPLACE = bin

VPATH += ..:

include $(SRCDIR)/../sources

OBJS += $(POSIX_OBJS)

##
## TODO: Figure this out, as this isn't right. Maybe create a new driver target
## so it's not necessary to override DriverEntry?
##

ENTRY := 0

TARGETLIBS = $(OBJROOT)/os/lib/yy/yy.a                     \
             $(OBJROOT)/os/lib/rtl/base/basertl.a          \
             $(OBJROOT)/os/lib/rtl/rtlc/rtlc.a             \

include $(SRCROOT)/os/minoca.mk


################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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


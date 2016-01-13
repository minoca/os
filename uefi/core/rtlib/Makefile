################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       UEFI Runtime Library
#
#   Abstract:
#
#       This file is responsible for building the core UEFI Runtime support,
#       which is compiled into most platform runtime images.
#
#   Author:
#
#       Evan Green 18-Mar-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = rtlib.a

BINARYTYPE = library

BINPLACE = intlib

INCLUDES += $(SRCROOT)/os/uefi/inc;$(SRCDIR)/..;

OBJS = crc32.o      \
       div.o        \
       init.o       \
       time.o       \
       util.o       \
       var.o        \

include $(SRCROOT)/os/minoca.mk

VPATH += $(SRCDIR)/..:

CFLAGS += -fshort-wchar


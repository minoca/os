################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       DbgExt
#
#   Abstract:
#
#       This module implements the import library for kernel debugger
#       extensions.
#
#   Author:
#
#       Evan Green 8-May-2013
#
#   Environment:
#
#       Debug Client
#
################################################################################

BINARY = dbgext.a

BINARYTYPE = library

BINPLACE = bin

INCLUDES += $(SRCDIR)/..;

OBJS = extimp.o   \

ifeq ($(OS),Windows_NT)

DIRS = win32

endif

include $(SRCROOT)/os/minoca.mk


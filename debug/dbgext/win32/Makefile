################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       DbgExtNt
#
#   Abstract:
#
#       This module implements the import library for kernel debugger
#       extensions on a Windows host.
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

BINARY = dbgextnt.a

BINARYTYPE = library

BINPLACE = bin

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

BUILD = yes

OBJS = extimp.o   \

include $(SRCROOT)/os/minoca.mk

##
## Override the compiler since this is a Windows library.
##

CC := gcc
LD := ld
RCC := windres
AR := ar
AS := as


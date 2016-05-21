################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Grammar Generator Library
#
#   Abstract:
#
#       This module implements support for an LALR(1) grammar data generator.
#
#   Author:
#
#       Evan Green 9-Apr-2016
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = yygen.a

BINARYTYPE = library

BUILD = yes

OBJS = lalr.o    \
       lr0.o     \
       output.o  \
       parcon.o  \
       verbose.o \
       yygen.o   \

include $(SRCROOT)/os/minoca.mk


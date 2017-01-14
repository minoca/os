################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       LZMA Library
#
#   Abstract:
#
#       This module contains support for compressing and decompressing using
#       the lzma algorithm.
#
#   Author:
#
#       Evan Green 27-Oct-2016
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = liblzma.a

BINARYTYPE = library

BINPLACE = bin

include $(SRCDIR)/sources

DIRS = build \
       util  \

include $(SRCROOT)/os/minoca.mk

util: $(BINARY)


################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Intel High Definition Audio
#
#   Abstract:
#
#       This module implements Intel High Definition Audio support.
#
#   Author:
#
#       Chris Stevens 3-Apr-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = intelhda.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = codec.o    \
       hda.o      \
       hdahw.o    \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/sound.drv           \

include $(SRCROOT)/os/minoca.mk


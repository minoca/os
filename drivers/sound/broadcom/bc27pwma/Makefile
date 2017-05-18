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
#       Broadcom 2709 PWM Audio
#
#   Abstract:
#
#       This module implements Broadcom 2709 PWM Audio support.
#
#   Author:
#
#       Chris Stevens 2-May-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = bc27pwma.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = pwma.o     \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/sound.drv           \

include $(SRCROOT)/os/minoca.mk


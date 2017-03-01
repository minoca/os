################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       SMSC 91C111
#
#   Abstract:
#
#       This module implements support for the SMSC91C111 ethernet controller.
#
#   Author:
#
#       Chris Stevens 16-Apr-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = smsc91c1.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = sm91c1.o    \
       sm91c1hw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk


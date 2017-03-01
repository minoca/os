################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       SMSC 95xx
#
#   Abstract:
#
#       This module implements support for the SMSC95xx family of USB ethernet
#       controllers.
#
#   Author:
#
#       Evan Green 7-Nov-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = smsc95xx.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = smsc95.o    \
       smsc95hw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk


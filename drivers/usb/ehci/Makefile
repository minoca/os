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
#       EHCI
#
#   Abstract:
#
#       This module implements the EHCI USB 2.0 Host Controller Driver.
#
#   Author:
#
#       Evan Green 18-Mar-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ehci.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = ehci.o     \
       ehcihc.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk


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
#       DWHCI
#
#   Abstract:
#
#       This module implements a DesignWare High-Speed USB 2.0 On-The-Go
#       (HS OTG) Host Controller Driver.
#
#   Author:
#
#       Chris Stevens 27-Mar-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = dwhci.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = dwhci.o     \
       dwhcihc.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk


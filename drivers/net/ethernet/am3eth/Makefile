################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       AM335x CPSW Ethernet Controller
#
#   Abstract:
#
#       This module implements the TI CPSW Ethernet Controller driver found on
#       TI AM335x devices.
#
#   Author:
#
#       Evan Green 20-Mar-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = am3eth.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = am3eth.o    \
       am3ethhw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk


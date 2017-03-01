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
#       ATL1c
#
#   Abstract:
#
#       This module implements a NIC driver for the Atheros ATL1C family
#       (formerly Attansic L1).
#
#   Author:
#
#       Evan Green 18-Apr-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = atl1c.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = atl1c.o   \
       atl1chw.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk


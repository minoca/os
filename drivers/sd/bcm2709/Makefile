################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All rights reserved.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       SD BCM2709
#
#   Abstract:
#
#       This module implements the BCM2709 SD controller driver.
#
#   Author:
#
#       Chris Stevens 10-Dec-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = sdbm2709.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = emmc.o       \
       sdbm2709.o   \

DYNLIBS = $(BINROOT)/kernel             \
          $(BINROOT)/sd.drv             \

include $(SRCROOT)/os/minoca.mk


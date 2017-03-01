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
#       RTL 81xx
#
#   Abstract:
#
#       This module implements support for the Realtek RTL81xx family of
#       ethernet controllers.
#
#   Author:
#
#       Chris Stevens 20-June-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rtl81xx.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = rtl81.o   \
       rtl81hw.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk


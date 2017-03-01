################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       AMD PCnet32
#
#   Abstract:
#
#       This module implements the Am79C9xx PCnet driver.
#
#   Author:
#
#       Chris Stevens 8-Nov-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = pcnet32.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = pcnet.o    \
       pcnethw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk


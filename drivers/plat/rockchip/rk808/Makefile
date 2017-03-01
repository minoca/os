################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       RK808
#
#   Abstract:
#
#       This module is the driver for the RK808 Power Management IC used in
#       platforms like the ASUS C201 Chromebook (Veyron Speedy).
#
#   Author:
#
#       Evan Green 4-Apr-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rk808.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = rk808.o   \

DYNLIBS = $(BINROOT)/kernel              \

include $(SRCROOT)/os/minoca.mk


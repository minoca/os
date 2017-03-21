################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Debug Client
#
#   Abstract:
#
#       This directory builds the various flavors of the debug client.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Debug Client
#
################################################################################

DIRS = minoca     \

ifeq ($(OS),Windows_NT)

DIRS += win32   \

else

DIRS += uos     \

endif

TESTDIRS = tdwarf   \
           teststab \
           testdisa

include $(SRCROOT)/os/minoca.mk

ifeq ($(OS),Windows_NT)

testdisa: win32
teststab: win32

endif


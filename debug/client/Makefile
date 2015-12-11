################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

endif

TESTDIRS = tdwarf   \
           teststab \
           testdisa

include $(SRCROOT)/os/minoca.mk

ifeq ($(OS),Windows_NT)

testdisa: win32
teststab: win32

endif


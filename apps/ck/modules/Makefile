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
#       Chalk Modules
#
#   Abstract:
#
#       This directory builds the Chalk modules.
#
#   Author:
#
#       Evan Green 28-Aug-2016
#
#   Environment:
#
#       User
#
################################################################################

DIRS = _time     \
       app       \
       bundle    \
       lzma      \
       json      \
       os        \
       spawn     \

include $(SRCROOT)/os/minoca.mk

CK_MODULES := bufferedio.ck \
              cpio.ck       \
              fileio.ck     \
              getopt.ck     \
              io.ck         \
              iobase.ck     \
              lzfile.ck     \
              time.ck       \

CK_LIB_PATH := $(BINROOT)/apps/usr/lib/chalk1
CK_BUILD_LIB_PATH := $(OUTROOT)/tools/lib/chalk1

CK_MODULE_TARGETS := $(CK_MODULES:%=$(CK_LIB_PATH)/%)
CK_MODULE_BUILD_TARGETS := $(CK_MODULES:%=$(CK_BUILD_LIB_PATH)/%)

all: $(CK_MODULE_TARGETS) $(CK_MODULE_BUILD_TARGETS)

##
## These static pattern rules in the form <1>: <2>: <3> say "for each target
## in <1>, apply pattern <2> to get prerequisites list <3> for that target."
##

$(CK_MODULE_TARGETS): $(CK_LIB_PATH)/%: %
	@mkdir -p $(CK_LIB_PATH)
	@cp -v $^ $@

$(CK_MODULE_BUILD_TARGETS): $(CK_BUILD_LIB_PATH)/%: %
	@mkdir -p $(CK_BUILD_LIB_PATH)
	@cp -v $^ $@


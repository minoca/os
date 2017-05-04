################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       mingen
#
#   Abstract:
#
#       This executable implements the Minoca build generator, which can create
#       Makefiles or build.ninja files out of build.ck directive files.
#
#   Author:
#
#       Evan Green 2-May-2017
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = mingen

OS ?= $(shell uname -s)

ifeq ($(OS),$(filter Windows_NT cygwin,$(OS)))

BINARY = mingen.exe

endif

BUILD = yes

BINPLACE = tools/bin

BINARYTYPE = custom

OBJS = make.ck     \
       mingen.ck   \
       mkbundle.ck \
       ninja.ck    \

include $(SRCROOT)/os/minoca.mk

BUILD_COMMAND = chalk $(SRCDIR)/mkbundle.ck $@

##
## This would be the recipe needed to rebuild the bootstrap Makefiles. It's a
## tad slow, and really is only needed once the (this) old system is gone. So
## its disabled out for now.
##

#postbuild: bootstrap/remakebs.sh
#	@echo Rebuilding bootstrap Makefiles
#	@sh $(SRCDIR)/bootstrap/remakebs.sh


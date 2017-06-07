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
#       _time (Build)
#
#   Abstract:
#
#       This Chalk module implements OS-level time functionality.
#
#   Author:
#
#       Evan Green 5-Jun-2017
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY := _time.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OS ?= $(shell uname -s)

ifeq ($(OS),$(filter Windows_NT cygwin,$(OS)))

OBJS += $(WIN32_OBJS)

else

OBJS += $(POSIX_OBJS)

endif

DIRS := dynamic \

include $(SRCROOT)/os/minoca.mk

dynamic: $(BINARY)


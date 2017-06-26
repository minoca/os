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
#       spawn (Dynamic)
#
#   Abstract:
#
#       This dynamic Chalk module implements subprocess functionality.
#
#   Author:
#
#       Evan Green 21-Jun-2017
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = spawn.so

BINARYTYPE = so

VPATH += ..:

include $(SRCDIR)/../sources

OBJS += $(POSIX_OBJS)

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/apps/usr/lib/chalk1
	@cp -p $(BINARY) $(BINROOT)/apps/usr/lib/chalk1/$(BINARY)

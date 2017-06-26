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
#       JSON (Dynamic)
#
#   Abstract:
#
#       This dynamic Chalk module implements JSON dumping and loading.
#
#   Author:
#
#       Evan Green 19-May-2017
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = json.so

BINARYTYPE = so

VPATH += ..:

include $(SRCDIR)/../sources

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/apps/usr/lib/chalk1
	@cp -p $(BINARY) $(BINROOT)/apps/usr/lib/chalk1/$(BINARY)


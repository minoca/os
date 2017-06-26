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
#       LZMA (Dynamic)
#
#   Abstract:
#
#       This Chalk module implements the LZMA compression library interface for
#       Chalk.
#
#   Author:
#
#       Evan Green 22-May-2017
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = lzma.so

BINARYTYPE = so

VPATH += ..:

include $(SRCDIR)/../sources

TARGETLIBS = $(OBJROOT)/os/apps/lib/lzma/liblzma.a \

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/apps/usr/lib/chalk1
	@cp -p $(BINARY) $(BINROOT)/apps/usr/lib/chalk1/$(BINARY)

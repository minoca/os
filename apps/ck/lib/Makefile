################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Chalk Library
#
#   Abstract:
#
#       This library contains the Chalk interpreter, which provides a
#       basic programming language.
#
#   Author:
#
#       Evan Green 9-Jun-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = libchalk.a

BINARYTYPE = library

include $(SRCDIR)/sources

OBJS += $(POSIX_OBJS)

DIRS = build \
       dynamic \
       gram  \

ENTRY := 0

include $(SRCROOT)/os/minoca.mk

dynamic: $(BINARY)
build: gram
gram.c: gram/gramgen.c | gram
	@echo Generating Grammar - $@
	@./gram/gramgen $@

ckcore.o: ckcore.ck


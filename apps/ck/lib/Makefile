################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Chalk
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

BINARY = chalk.a

BINARYTYPE = library

include $(SRCDIR)/sources

DIRS = build \
       gram

include $(SRCROOT)/os/minoca.mk

build: gram
gram.c: gram/gramgen.c
	@echo Generating Grammar - $@
	@./gram/gramgen $@


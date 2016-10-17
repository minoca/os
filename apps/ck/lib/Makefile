################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
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


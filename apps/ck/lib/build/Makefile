################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Chalk Library (Build)
#
#   Abstract:
#
#       This library contains the Chalk interpreter targeted toward the build
#       machine environment.
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

BUILD = yes

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

include $(SRCROOT)/os/minoca.mk

gram.c:
	@echo Generating Grammar - $@
	@../gram/gramgen $@

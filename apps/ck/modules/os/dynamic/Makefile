################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       OS (Dynamic)
#
#   Abstract:
#
#       This dynamic Chalk module implements core OS level functionality.
#
#   Author:
#
#       Evan Green 28-Aug-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = os.so

BINARYTYPE = so

VPATH += ..:

include $(SRCDIR)/../sources

# OBJS += $(POSIX_OBJS)

ENTRY := 0

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/chalk1
	@cp -p $(BINARY) $(BINROOT)/chalk1/$(BINARY)


################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       OS
#
#   Abstract:
#
#       This Chalk module implements core OS level functionality.
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

BINARY = os.a

BINARYTYPE = library

include $(SRCDIR)/sources

# OBJS += $(POSIX_OBJS)

DIRS = build   \
       dynamic \

include $(SRCROOT)/os/minoca.mk

dynamic: $(BINARY)


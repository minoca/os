################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       bundle (Build)
#
#   Abstract:
#
#       This Chalk module implements support for creating a single specialized
#       binary out of an existing Chalk environment.
#
#   Author:
#
#       Evan Green 19-Oct-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY := bundle.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

include $(SRCROOT)/os/minoca.mk


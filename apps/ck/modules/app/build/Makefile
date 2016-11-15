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
#       app (Build)
#
#   Abstract:
#
#       This Chalk module implements the app interface module.
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

BINARY := app.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

include $(SRCROOT)/os/minoca.mk


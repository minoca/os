################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Chalk (Build)
#
#   Abstract:
#
#       This library contains the Chalk interpreter built in the build machine
#       environment.
#
#   Author:
#
#       Evan Green 19-Nov-2015
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


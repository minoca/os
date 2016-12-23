################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Chalk
#
#   Abstract:
#
#       This directory builds the Chalk language library and command line
#       app.
#
#   Author:
#
#       Evan Green 9-Jun-2016
#
#   Environment:
#
#       User
#
################################################################################

DIRS = app       \
       lib       \
       modules   \

include $(SRCROOT)/os/minoca.mk

modules: lib
app: modules lib


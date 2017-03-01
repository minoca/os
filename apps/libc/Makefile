################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       C Library
#
#   Abstract:
#
#       This module contains the C library, which provides a largely standard
#       interface between applications and the kernel interface. This C library
#       is considered "mostly POSIX". It attempts to follow the POSIX standard
#       as closely as possible, but adds many extensions expected by mainstream
#       programs, and omits some of the darker corners of the specification.
#
#   Author:
#
#       Evan Green 4-Mar-2013
#
#   Environment:
#
#       Build
#
################################################################################

DIRS = crypt    \
       dynamic  \
       static   \

LDFLAGS := -nostdlib

include $(SRCROOT)/os/minoca.mk

crypt: dynamic


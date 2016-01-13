################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

include $(SRCROOT)/os/minoca.mk

crypt: dynamic


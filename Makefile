################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Minoca OS
#
#   Abstract:
#
#       This file is responsible for kicking off the build of all source
#       directories.
#
#   Author:
#
#       Evan Green 19-Jun-2012
#
#   Environment:
#
#       Build
#
################################################################################

##
## Check for the necessary environment variables.
##

ifndef SRCROOT
$(error Error: Environment not set up: SRCROOT environment variable missing)
endif

ifndef ARCH
$(error Error: Environment not set up: ARCH environment variable missing)
endif

ifndef DEBUG
$(error Error: Environment not set up: DEBUG environment variable missing)
endif

LIBRARIES = lib          \

COMPONENTS = apps        \
             boot        \
             drivers     \
             kernel      \
             uefi        \

DIRS = $(LIBRARIES)      \
       $(COMPONENTS)     \
       images

include $(SRCROOT)/os/minoca.mk

images: $(COMPONENTS)
$(COMPONENTS): $(LIBRARIES)

##
## Boot and UEFI need KD from the kernel (which could be moved out to a
## library). Drivers and apps (which may also build some drivers) link against
## the kernel.
##

boot drivers apps uefi: kernel

##
## Boot relies on UEFI for the elfconv tool.
##

boot: uefi


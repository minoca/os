################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

DIRS = apps        \
       createimage \
       boot        \
       debug       \
       drivers     \
       lib         \
       kernel      \
       tzcomp      \
       uefi        \

include $(SRCROOT)/os/minoca.mk

kernel: lib
boot: kernel lib uefi
drivers: kernel boot lib
apps: kernel
createimage: kernel boot drivers apps debug uefi tzcomp
debug: kernel apps
uefi: kernel lib


################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Applications
#
#   Abstract:
#
#       This module contains user-mode programs.
#
#   Author:
#
#       Evan Green 25-Feb-2013
#
#   Environment:
#
#       Build
#
################################################################################

##
## Add apps that are dependent on the C library here.
##

APPS = efiboot  \
       mount    \
       profile  \
       setup    \
       swiss    \
       testapps \
       unmount  \
       vmstat   \

DIRS = $(APPS)  \
       lib      \
       libc     \
       osbase   \
       posix    \

include $(SRCROOT)/os/minoca.mk

libc: osbase
$(APPS): libc lib
lib: libc


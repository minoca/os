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

APPS = ck       \
       debug    \
       efiboot  \
       mingen   \
       mount    \
       netcon   \
       profile  \
       setup    \
       swiss    \
       testapps \
       tzcomp   \
       tzset    \
       unmount  \
       vmstat   \

DIRS = $(APPS)  \
       lib      \
       libc     \
       netlink  \
       osbase   \
       posix    \

include $(SRCROOT)/os/minoca.mk

libc: osbase
$(APPS): libc lib netlink
lib: libc
netlink: libc
setup mingen: ck


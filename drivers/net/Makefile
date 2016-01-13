################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Networking
#
#   Abstract:
#
#       This directory contains networking-related drivers, including the
#       networking core driver and support for many specific NICs.
#
#   Author:
#
#       Evan Green 4-Apr-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

NIC_DRIVERS = am3eth    \
              atl1c     \
              dwceth    \
              e100      \
              rtl81xx   \
              smsc91c1  \
              smsc95xx  \

DIRS = netcore          \
       $(NIC_DRIVERS)   \

include $(SRCROOT)/os/minoca.mk

$(NIC_DRIVERS): netcore


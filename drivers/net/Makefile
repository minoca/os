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

DIRS = ethernet \
       netcore  \
       net80211 \
       wireless \

include $(SRCROOT)/os/minoca.mk

ethernet net80211 wireless: netcore
wireless: net80211


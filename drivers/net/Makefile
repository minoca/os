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


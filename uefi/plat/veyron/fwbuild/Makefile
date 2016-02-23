################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       VeyrnFwb
#
#   Abstract:
#
#       This build utility adds the necessary keyblock and preamble for booting
#       a firmware image on a Rk3288 Veyron SoC.
#
#   Author:
#
#       Chris Stevens 7-Jul-2015
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = veyrnfwb

BUILD = yes

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/uefi/include;

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a          \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a             \
             $(OBJROOT)/os/lib/crypto/build/crypto.a             \
             $(OBJROOT)/os/lib/crypto/ssl/build/ssl.a            \

OBJS = fwbuild.o

include $(SRCROOT)/os/minoca.mk


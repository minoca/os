/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    config.S

Abstract:

    This module includes the configuration scripts directly in the binary.

Author:

    Evan Green 4-Oct-2016

Environment:

    Any

--*/

##
## ---------------------------------------------------------------- Definitions
##

#ifdef __WINNT

#define LABEL(_Name) _##_Name

#else

#define LABEL(_Name) _Name

#endif

#define INCLUDE_BINARY(_Path, _Name) \
    .global LABEL(_Name##_start); \
    LABEL(_Name##_start):; \
    .incbin _Path; \
    .global LABEL(_Name##_end); \
    LABEL(_Name##_end):

##
## ---------------------------------------------------------------------- Code
##

.text

INCLUDE_BINARY("bbone.txt", _binary_bbone_txt)
INCLUDE_BINARY("common.txt", _binary_common_txt)
INCLUDE_BINARY("galileo.txt", _binary_galileo_txt)
INCLUDE_BINARY("instarv6.txt", _binary_instarv6_txt)
INCLUDE_BINARY("instarv7.txt", _binary_instarv7_txt)
INCLUDE_BINARY("instx86.txt", _binary_instx86_txt)
INCLUDE_BINARY("integrd.txt", _binary_integrd_txt)
INCLUDE_BINARY("panda.txt", _binary_panda_txt)
INCLUDE_BINARY("pandausb.txt", _binary_pandausb_txt)
INCLUDE_BINARY("pc.txt", _binary_pc_txt)
INCLUDE_BINARY("pcefi.txt", _binary_pcefi_txt)
INCLUDE_BINARY("pctiny.txt", _binary_pctiny_txt)
INCLUDE_BINARY("rpi.txt", _binary_rpi_txt)
INCLUDE_BINARY("rpi2.txt", _binary_rpi2_txt)
INCLUDE_BINARY("veyron.txt", _binary_veyron_txt)

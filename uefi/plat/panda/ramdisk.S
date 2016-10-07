/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    ramdisk.S

Abstract:

    This module embeds the RAM disk image directly in the firmware image.

Author:

    Evan Green 4-Oct-2016

Environment:

    Any

--*/

##
## ---------------------------------------------------------------- Definitions
##

#if defined(__WINNT) || defined(__APPLE__)

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

INCLUDE_BINARY("ramdisk", _binary_ramdisk)


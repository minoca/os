/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    emptyrd.S

Abstract:

    This module adds the symbols for an empty RAM disk.

Author:

    Evan Green 5-Oct-2016

Environment:

    Any

--*/

##
## ---------------------------------------------------------------------- Code
##

.text

.global _binary_ramdisk_start
_binary_ramdisk_start:
.global _binary_ramdisk_end
_binary_ramdisk_end:
.byte 0


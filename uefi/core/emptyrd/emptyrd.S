/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements x64 architecture specific support routines.

Author:

    Evan Green 11-Aug-2017

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/x64.inc>

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// VOID
// EfiMemoryBarrier (
//     VOID
//     )
//

/*++

Routine Description:

    This routine provides a full memory barrier, ensuring that all memory
    accesses occurring before this function complete before any memory accesses
    after this function start.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION(EfiMemoryBarrier)
    lock orq    $0, (%rsp)
    retq

END_FUNCTION(EfiMemoryBarrier)

//
// --------------------------------------------------------- Internal Functions
//


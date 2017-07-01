/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsup.S

Abstract:

    This module implements assembly-based architecture support routines for the
    ARMv6 platform.

Author:

    Chris Stevens 20-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/arm.inc>

//
// ---------------------------------------------------------------- Definitions
//

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

FUNCTION EfiMemoryBarrier
    mcr     p15, 0, %r0, %cr7, %cr10, 5
    bx      %lr

END_FUNCTION EfiMemoryBarrier

//
// VOID
// EfipCleanEntireCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine cleans the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION EfipCleanEntireCache
    mov     %r1, #0
    mcr     p15, 0, %r1, %cr7, %cr10, 0 @ Clean entire data cache.
    mcr     p15, 0, %r1, %cr7, %cr10, 4 @ Data Synchronization barrier.
    bx      %lr

END_FUNCTION EfipCleanEntireCache

//
// VOID
// EfipInvalidateInstructionCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine invalidate the processor's instruction only cache, indicating
    that a page containing code has changed.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION EfipInvalidateInstructionCache
    mov     %r1, #0
    mcr     p15, 0, %r1, %cr7, %cr10, 4 @ Data synchronization barrier.
    mcr     p15, 0, %r1, %cr7, %cr5, 0  @ ICIALLU, Invalidate I-Cache.
    mcr     p15, 0, %r1, %cr7, %cr10, 4 @ DSB, Make instructions finish.
    mcr     p15, 0, %r1, %cr7, %cr5, 4  @ ISB, Prevent speculative fetching.
    bx      %lr                         @ Return

END_FUNCTION EfipInvalidateInstructionCache

//
// --------------------------------------------------------- Internal Functions
//


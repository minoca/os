/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdsup.S

Abstract:

    This module implements ARMv7 specific support routines for the Kernel
    Debugging subsystem.

Author:

    Evan Green 21-Aug-2012

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------ Includes
//

#include <minoca/kernel/arm.inc>

//
// --------------------------------------------------------------- Definitions
//

//
// ---------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// VOID
// KdpBreak (
//     VOID
//     )
//

/*++

Routine Description:

    This routine causes a break into the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION KdpBreak
    DEBUGGER_BREAK                              @ Debugger break.
    bx  %lr                                     @ Return.

END_FUNCTION KdpBreak

//
// VOID
// KdpCleanMemory (
//     PVOID Address
//     )
//

/*++

Routine Description:

    This routine cleans memory modified by the kernel debugger, flushing it
    out of the instruciton and data caches.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned.

Return Value:

    None.

--*/

FUNCTION KdpCleanMemory
    mcr     p15, 0, %r0, c7, c10, 1             @ DCCMVAC, Clean the cache line.
    mov     %r0, #0                             @
    mcr     p15, 0, %r0, c7, c5, 0              @ ICIALLU, Invalidate i-cache.
    DSB                                         @ Make instructions finish.
    ISB                                         @ Prevent speculative fetching.
    bx      %lr                                 @

END_FUNCTION KdpCleanMemory

//
// VOID
// KdpInvalidateInstructionCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine invalidates the instruction cache to PoU inner shareable.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION KdpInvalidateInstructionCache
    mov     %r0, #0                             @
    mcr     p15, 0, %r0, c7, c5, 0              @ ICIALLU - Invalidate i-cache.
    DSB                                         @ Make instructions finish.
    ISB                                         @ Prevent speculative fetching.
    bx      %lr                                 @

END_FUNCTION KdpInvalidateInstructionCache

//
// VOID
// KdpDisableInterrupts (
//     VOID
//     )
//

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION KdpDisableInterrupts
    cpsid   i                       @ OR in the interrupts flag.
    bx      %lr                     @

END_FUNCTION KdpDisableInterrupts

//
// --------------------------------------------------------- Internal Functions
//


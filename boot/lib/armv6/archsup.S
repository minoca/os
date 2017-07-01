/*++

Copyright (c) 2013 Minoca Corp.

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

    Chris Stevens 2-Feb-2014

Environment:

    Boot

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
// BoInvalidateEntireCache (
//     VOID
//     )
//

/*++

Routine Description:

    This routine invalidates the entire data cache.

Arguments:

    None.

Return Value:

    None.

--*/

FUNCTION BoInvalidateEntireCache
    mov     %r1, #0
    mcr     p15, 0, %r1, %cr7, %cr6, 0  @ Invalidate entire data cache.
    mcr     p15, 0, %r1, %cr7, %cr10, 4 @ Data Synchronization barrier.
    bx      %lr

END_FUNCTION BoInvalidateEntireCache

//
// ULONG
// ArGetMultiprocessorIdRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the Multiprocessor ID register (MPIDR).

Arguments:

    None.

Return Value:

    Returns the value of the MPIDR.

--*/

FUNCTION ArGetMultiprocessorIdRegister

    //
    // The MPIDR does not exist on ARMv6; return 0.
    //

    mov     %r0, #0
    bx      %lr

END_FUNCTION ArGetMultiprocessorIdRegister

//
// ULONG
// ArGetPerformanceControlRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrieves the PMCR (Performance Monitor Control Register).

Arguments:

    None.

Return Value:

    Returns the value of the PMCR.

--*/

FUNCTION ArGetPerformanceControlRegister
    mrc     p15, 0, %r0, %c15, %c12, 0          @ Get the PMCR.
    bx      %lr                                 @

END_FUNCTION ArGetPerformanceControlRegister

//
// VOID
// ArSetPerformanceControlRegister (
//     ULONG Value
//     )
//

/*++

Routine Description:

    This routine sets the PMCR (Performance Monitor Control Register).

Arguments:

    Value - Supplies the value to set in the PMCR.

Return Value:

    None.

--*/

FUNCTION ArSetPerformanceControlRegister
    mcr     p15, 0, %r0, %c15, %c12, 0          @ Set the PMCR.
    bx      %lr                                 @

END_FUNCTION ArSetPerformanceControlRegister

//
// ULONG
// ArGetCycleCountRegister (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrieves the PMCCNTR (Performance Monitor Cycle Counter)
    register.

Arguments:

    None.

Return Value:

    Returns the value of the PMCCNTR.

--*/

FUNCTION ArGetCycleCountRegister
    mrc     p15, 0, %r0, %c15, %c12, 1          @ Get the PMCCNTR register.
    bx      %lr                                 @

END_FUNCTION ArGetCycleCountRegister

//
// --------------------------------------------------------- Internal Functions
//


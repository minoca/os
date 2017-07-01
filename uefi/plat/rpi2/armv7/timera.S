/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timera.S

Abstract:

    This module implements assembly routines for BCM2836 timers.

Author:

    Chris Stevens 14-Jun-2016

Environment:

    Firmware

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
// UINT32
// EfipBcm2836GetGtFrequency (
//     VOID
//     )
//

/*++

Routine Description:

    This routine gets the ARM Generic Timer's frequency (CNTFRQ).

Arguments:

    None.

Return Value:

    Returns the value of the CNTFRQ.

--*/

FUNCTION EfipBcm2836GetGtFrequency
    mrc     p15, 0, %r0, %c14, %c0, 0          @ Get the CNTFRQ
    bx      %lr                                @

END_FUNCTION EfipBcm2836GetGtFrequency

//
// --------------------------------------------------------- Internal Functions
//


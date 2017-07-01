/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gta.S

Abstract:

    This module implements support for accessing ARM Generic Timer registers.

Author:

    Chris Stevens 9-Jun-2016

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
// VOID
// EfipGtSetVirtualTimerControl (
//     UINT32 Control
//     )
//

/*++

Routine Description:

    This routine sets the CNTV_CTL register.

Arguments:

    Control - Supplies the control value to set in the CNTV_CTL.

Return Value:

    None.

--*/

FUNCTION EfipGtSetVirtualTimerControl
    mcr     p15, 0, %r0, %c14, %c3, 1          @ Set the CNTV_CTL
    bx      %lr                                @

END_FUNCTION EfipGtSetVirtualTimerControl

//
// UINT64
// EfipGtGetVirtualCount (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrieves the CNTVCT register.

Arguments:

    None.

Return Value:

    Returns the value of the CNTVCT.

--*/

FUNCTION EfipGtGetVirtualCount
    mrrc    p15, 1, %r0, %r1, %c14             @ Get the CNTVCT
    bx      %lr                                @

END_FUNCTION EfipGtGetVirtualCount

//
// VOID
// EfipGtSetVirtualTimerCompare (
//     UINT64 CompareValue
//     )
//

/*++

Routine Description:

    This routine retrieves the CNTV_CVAL register.

Arguments:

    CompareValue - Supplies the compare value to set in the CNTV_CVAL register.

Return Value:

    None.

--*/

FUNCTION EfipGtSetVirtualTimerCompare
    mcrr    p15, 3, %r0, %r1, %c14             @ Set the CNTV_CVAL
    bx      %lr                                @

END_FUNCTION EfipGtSetVirtualTimerCompare

//
// --------------------------------------------------------- Internal Functions
//



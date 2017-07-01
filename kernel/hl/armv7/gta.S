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

    Chris Stevens 23-May-2016

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
// ULONG
// HlpGtGetFrequency (
//     VOID
//     )
//

/*++

Routine Description:

    This routine retrieves the CNTFRQ register.

Arguments:

    None.

Return Value:

    Returns the value of the CNTFRQ.

--*/

FUNCTION HlpGtGetFrequency
    mrc     p15, 0, %r0, %c14, %c0, 0          @ Get the CNTFRQ
    bx      %lr                                @

END_FUNCTION HlpGtGetFrequency

//
// VOID
// HlpGtSetVirtualTimerControl (
//     ULONG Control
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

FUNCTION HlpGtSetVirtualTimerControl
    mcr     p15, 0, %r0, %c14, %c3, 1          @ Set the CNTV_CTL
    bx      %lr                                @

END_FUNCTION HlpGtSetVirtualTimerControl

//
// ULONGLONG
// HlpGtGetVirtualCount (
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

FUNCTION HlpGtGetVirtualCount
    mrrc    p15, 1, %r0, %r1, %c14             @ Get the CNTVCT
    bx      %lr                                @

END_FUNCTION HlpGtGetVirtualCount

//
// VOID
// HlpGtSetVirtualTimerCompare (
//     ULONGLONG CompareValue
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

FUNCTION HlpGtSetVirtualTimerCompare
    mcrr    p15, 3, %r0, %r1, %c14             @ Set the CNTV_CVAL
    bx      %lr                                @

END_FUNCTION HlpGtSetVirtualTimerCompare

//
// --------------------------------------------------------- Internal Functions
//



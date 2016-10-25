/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    clock.h

Abstract:

    This header contains definitions for the hardware layer's clock interrupt
    support.

Author:

    Evan Green 19-Aug-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------- Function Prototypes
//

INTERRUPT_STATUS
HlpEarlyClockInterruptHandler (
    PVOID Context
    );

/*++

Routine Description:

    This routine responds to clock interrupts while the system is still in
    early initialization.

Arguments:

    Context - Supplies a context pointer. Currently unused.

Return Value:

    Claimed always.

--*/

INTERRUPT_STATUS
HlpClockIpiHandler (
    PVOID Context
    );

/*++

Routine Description:

    This routine is the ISR for clock IPIs. The main difference being that it
    does not need to acknowledge the clock interrupt in the hardware module as
    this interrupt is software generated.

Arguments:

    Context - Supplies a context pointer. Currently unused.

Return Value:

    Claimed always.

--*/

KSTATUS
HlpTimerInitializeClock (
    VOID
    );

/*++

Routine Description:

    This routine initializes the system clock source and start it ticking.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
HlpTimerActivateClock (
    VOID
    );

/*++

Routine Description:

    This routine sets the clock handler routine to the main clock ISR.

Arguments:

    None.

Return Value:

    Status code.

--*/


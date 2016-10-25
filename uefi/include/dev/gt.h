/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gt.h

Abstract:

    This header contains definitions for the ARM Generic Timer.

Author:

    Chris Stevens 9-Jun-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the Generic Timer context.

Members:

    Period - Stores the interrupt period in timer ticks.

    DueTime - Stores the current absolute time the timer is due to interrupt.

--*/

typedef struct _GT_CONTEXT {
    UINT64 Period;
    UINT64 DueTime;
} GT_CONTEXT, *PGT_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFI_STATUS
EfipGtInitialize (
    PGT_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes an ARM Generic Timer.

Arguments:

    Context - Supplies a pointer to the timer context.

Return Value:

    EFI Status code.

--*/

UINT64
EfipGtRead (
    PGT_CONTEXT Context
    );

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Context - Supplies a pointer to the timer context.

Return Value:

    Returns the timer's current count.

--*/

EFI_STATUS
EfipGtArm (
    PGT_CONTEXT Context,
    BOOLEAN Periodic,
    UINT64 TickCount
    );

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Context - Supplies a pointer to the timer context.

    Periodic - Supplies a boolean indicating whether or not the timer should
        interrupt periodically or just once.

    TickCount - Supplies the number of timer ticks from now for the timer to
        fire in.

Return Value:

    EFI Status code.

--*/

VOID
EfipGtDisarm (
    PGT_CONTEXT Context
    );

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies a pointer to the timer context.

Return Value:

    None.

--*/

VOID
EfipGtAcknowledgeInterrupt (
    PGT_CONTEXT Context
    );

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Context - Supplies a pointer to the timer context.

Return Value:

    None.

--*/


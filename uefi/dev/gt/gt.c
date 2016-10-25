/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gt.c

Abstract:

    This module implements timer support for the ARM Generic Timer.

Author:

    Chris Stevens 9-Jun-2016

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "dev/gt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the bits for a generic timer control register.
//

#define GT_CONTROL_INTERRUPT_STATUS_ASSERTED 0x00000004
#define GT_CONTROL_INTERRUPT_MASKED          0x00000002
#define GT_CONTROL_TIMER_ENABLE              0x00000001

//
// --------------------------------------------------------------------- Macros
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipGtSetVirtualTimerControl (
    UINT32 Control
    );

UINT64
EfipGtGetVirtualCount (
    VOID
    );

VOID
EfipGtSetVirtualTimerCompare (
    UINT64 CompareValue
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipGtInitialize (
    PGT_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes an ARM Generic Timer.

Arguments:

    Context - Supplies a pointer to the timer context.

Return Value:

    EFI Status code.

--*/

{

    //
    // The timer is already running, just make sure interrupts are off.
    //

    Context->Period = 0;
    EfipGtSetVirtualTimerControl(0);
    return EFI_SUCCESS;
}

UINT64
EfipGtRead (
    PGT_CONTEXT Context
    )

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Context - Supplies a pointer to the timer context.

Return Value:

    Returns the timer's current count.

--*/

{

    return EfipGtGetVirtualCount();
}

EFI_STATUS
EfipGtArm (
    PGT_CONTEXT Context,
    BOOLEAN Periodic,
    UINT64 TickCount
    )

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

{

    UINT64 DueTime;
    BOOLEAN Enabled;

    //
    // In order to synchronize with the rearming of the timer during
    // acknowledge interrupt, perform the arm with interrupts disabled.
    //

    Enabled = EfiDisableInterrupts();

    //
    // The tick count is relative in both modes, but the GT can only be
    // armed with an absolute time. Add the current time.
    //

    DueTime = TickCount + EfipGtGetVirtualCount();
    if (Periodic != FALSE) {
        Context->Period = TickCount;
        Context->DueTime = DueTime;

    } else {
        Context->Period = 0;
    }

    EfipGtSetVirtualTimerCompare(DueTime);
    EfipGtSetVirtualTimerControl(GT_CONTROL_TIMER_ENABLE);
    if (Enabled != FALSE) {
        EfiEnableInterrupts();
    }

    return EFI_SUCCESS;
}

VOID
EfipGtDisarm (
    PGT_CONTEXT Context
    )

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies a pointer to the timer context.

Return Value:

    None.

--*/

{

    BOOLEAN Enabled;

    //
    // In order to synchronize with the rearming of the timer during
    // acknowledge interrupt, perform the disarm with interrupts disabled.
    //

    Enabled = EfiDisableInterrupts();
    Context->Period = 0;
    EfipGtSetVirtualTimerControl(0);
    if (Enabled != FALSE) {
        EfiEnableInterrupts();
    }

    return;
}

VOID
EfipGtAcknowledgeInterrupt (
    PGT_CONTEXT Context
    )

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

{

    UINT64 DueTime;

    //
    // The only way to stop an interrupt from continuing to fire is to either
    // reprogram the compare register or to disable the interrupt. As the timer
    // must await further instruction, disable the interrupt.
    //

    EfipGtSetVirtualTimerControl(0);
    if (Context->Period != 0) {
        DueTime = Context->DueTime + Context->Period;
        Context->DueTime = DueTime;
        EfipGtSetVirtualTimerCompare(DueTime);
        EfipGtSetVirtualTimerControl(GT_CONTROL_TIMER_ENABLE);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


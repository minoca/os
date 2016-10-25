/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements support for the BCM2709 timer services.

Author:

    Chris Stevens 18-Mar-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from a BCM2709 ARM timer.
//

#define READ_ARM_TIMER_REGISTER(_Register) \
    EfiReadRegister32(BCM2709_ARM_TIMER_BASE + (_Register))

//
// This macro writes to a BCM2709 ARM timer.
//

#define WRITE_ARM_TIMER_REGISTER(_Register, _Value) \
    EfiWriteRegister32(BCM2709_ARM_TIMER_BASE + (_Register), (_Value))

//
// This macro reads from a BCM2709 System timer.
//

#define READ_SYSTEM_TIMER_REGISTER(_Register) \
    EfiReadRegister32(BCM2709_SYSTEM_TIMER_BASE + (_Register))

//
// This macro writes to a BCM2709 System timer.
//

#define WRITE_SYSTEM_TIMER_REGISTER(_Register, _Value) \
    EfiWriteRegister32(BCM2709_SYSTEM_TIMER_BASE + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709TimerInitialize (
    PBCM2709_TIMER Timer
    )

/*++

Routine Description:

    This routine initializes a BCM2709 timer.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    Status code.

--*/

{

    UINT32 ControlValue;

    //
    // The BCM2709 device library must be initialized first.
    //

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    //
    // Program the default timer with no interrupt generation. There is nothing
    // to be done for the System Timer's free-running counter. It is already
    // enabled.
    //

    if (Timer->ClockTimer != FALSE) {
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerPredivider, Timer->Predivider);
        ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
        ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE;
        ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                         BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                         BCM2709_ARM_TIMER_CONTROL_32_BIT);

        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
        WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
    }

    return EFI_SUCCESS;
}

UINT64
EfipBcm2709TimerRead (
    PBCM2709_TIMER Timer
    )

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    Returns the timer's current count.

--*/

{

    UINT32 Value;

    if (Timer->ClockTimer != FALSE) {
        Value = 0xFFFFFFFF;
        Value -= READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerCurrentValue);

    } else {
        Value = READ_SYSTEM_TIMER_REGISTER(Bcm2709SystemTimerCounterLow);
    }

    return Value;
}

VOID
EfipBcm2709TimerArm (
    PBCM2709_TIMER Timer,
    UINT64 TickCount
    )

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Timer - Supplies the pointer to the timer data.

    Periodic - Supplies a boolean indicating if the timer should be armed
        periodically or one-shot.

    TickCount - Supplies the interval, in ticks, from now for the timer to fire
        in.

Return Value:

    None.

--*/

{

    UINT32 ControlValue;

    if (Timer->ClockTimer == FALSE) {
        return;
    }

    //
    // Set up the control value to program.
    //

    ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
    ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                     BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                     BCM2709_ARM_TIMER_CONTROL_32_BIT |
                     BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE);

    //
    // Set the timer to its maximum value, set the configuration, clear the
    // interrupt, then set the value.
    //

    WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerLoadValue, 0xFFFFFFFF);
    WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
    WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
    WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerLoadValue, TickCount);
    return;
}

VOID
EfipBcm2709TimerDisarm (
    PBCM2709_TIMER Timer
    )

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    None.

--*/

{

    UINT32 ControlValue;

    if (Timer->ClockTimer == FALSE) {
        return;
    }

    //
    // Disable the timer by programming it with no interrupt generation.
    //

    ControlValue = READ_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl);
    ControlValue &= ~BCM2709_ARM_TIMER_CONTROL_INTERRUPT_ENABLE;
    ControlValue |= (BCM2709_ARM_TIMER_CONTROL_ENABLED |
                     BCM2709_ARM_TIMER_CONTROL_DIVIDE_BY_1 |
                     BCM2709_ARM_TIMER_CONTROL_32_BIT);

    WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerControl, ControlValue);
    WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
    return;
}

VOID
EfipBcm2709TimerAcknowledgeInterrupt (
    PBCM2709_TIMER Timer
    )

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    None.

--*/

{

    if (Timer->ClockTimer == FALSE) {
        return;
    }

    WRITE_ARM_TIMER_REGISTER(Bcm2709ArmTimerInterruptClear, 1);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


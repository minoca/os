/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements platform timer services for the ARM Integrator/CP.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "integfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an Integrator/CP timer.
//

#define READ_TIMER_REGISTER(_Controller, _Register) \
    EfiReadRegister32((_Controller)->BaseAddress + (_Register))

//
// This macro writes to an Integrator/CP timer.
//

#define WRITE_TIMER_REGISTER(_Controller, _Register, _Value) \
    EfiWriteRegister32((_Controller)->BaseAddress + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define INTEGRATOR_IRQ_TIMER0 5
#define INTEGRATOR_IRQ_TIMER1 6
#define INTEGRATOR_IRQ_TIMER2 7

#define INTEGRATOR_TIMER_BASE 0x13000000

//
// The second and third timers run at a fixed frequency (the first runs at the
// system clock speed).
//

#define INTEGRATOR_TIMER_FREQUENCY 1000000

//
// Run at at a period of 15.625ms.
//

#define INTEGRATOR_CLOCK_TICK_COUNT 15625

//
// Control register bits.
//

#define INTEGRATOR_TIMER_ENABLED           0x00000080
#define INTEGRATOR_TIMER_MODE_FREE_RUNNING 0x00000000
#define INTEGRATOR_TIMER_MODE_PERIODIC     0x00000040
#define INTEGRATOR_TIMER_INTERRUPT_ENABLE  0x00000020
#define INTEGRATOR_TIMER_DIVIDE_BY_1       0x00000000
#define INTEGRATOR_TIMER_DIVIDE_BY_16      0x00000004
#define INTEGRATOR_TIMER_DIVIDE_BY_256     0x00000008
#define INTEGRATOR_TIMER_32_BIT            0x00000002
#define INTEGRATOR_TIMER_16_BIT            0x00000000
#define INTEGRATOR_TIMER_MODE_ONE_SHOT     0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the registers for one timer, in bytes.
//

typedef enum _INTEGRATOR_TIMER_REGISTER {
    IntegratorTimerLoadValue           = 0x0,
    IntegratorTimerCurrentValue        = 0x4,
    IntegratorTimerControl             = 0x8,
    IntegratorTimerInterruptClear      = 0xC,
    IntegratorTimerInterruptRawStatus  = 0x10,
    IntegratorTimerInterruptStatus     = 0x14,
    IntegratorTimerBackgroundLoadValue = 0x18,
    IntegratorTimerRegisterSize        = 0x100
} INTEGRATOR_TIMER_REGISTER, *PINTEGRATOR_TIMER_REGISTER;

/*++

Structure Description:

    This structure stores the internal state associated with a Integrator/CP
    timer.

Members:

    BaseAddress - Stores the virtual address of the beginning of this timer
        block.

    Index - Stores the zero-based index of this timer within the timer block.

--*/

typedef struct _INTEGRATOR_TIMER_DATA {
    VOID *BaseAddress;
    UINT32 Index;
} INTEGRATOR_TIMER_DATA, *PINTEGRATOR_TIMER_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipPlatformServiceTimerInterrupt (
    UINT32 InterruptNumber
    );

UINT64
EfipPlatformReadTimer (
    VOID
    );

VOID
EfipIntegratorTimerInitialize (
    PINTEGRATOR_TIMER_DATA Timer
    );

UINT64
EfipIntegratorTimerRead (
    PINTEGRATOR_TIMER_DATA Timer
    );

VOID
EfipIntegratorTimerArm (
    PINTEGRATOR_TIMER_DATA Timer,
    BOOLEAN Periodic,
    UINT64 TickCount
    );

VOID
EfipIntegratorTimerDisarm (
    PINTEGRATOR_TIMER_DATA Timer
    );

VOID
EfipIntegratorTimerAcknowledgeInterrupt (
    PINTEGRATOR_TIMER_DATA Timer
    );

//
// -------------------------------------------------------------------- Globals
//

INTEGRATOR_TIMER_DATA EfiIntegratorClockTimer;
INTEGRATOR_TIMER_DATA EfiIntegratorTimeCounter;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiPlatformSetWatchdogTimer (
    UINTN Timeout,
    UINT64 WatchdogCode,
    UINTN DataSize,
    CHAR16 *WatchdogData
    )

/*++

Routine Description:

    This routine sets the system's watchdog timer.

Arguments:

    Timeout - Supplies the number of seconds to set the timer for.

    WatchdogCode - Supplies a numeric code to log on a watchdog timeout event.

    DataSize - Supplies the size of the watchdog data.

    WatchdogData - Supplies an optional buffer that includes a null-terminated
        string, optionally followed by additional binary data.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the supplied watchdog code is invalid.

    EFI_UNSUPPORTED if there is no watchdog timer.

    EFI_DEVICE_ERROR if an error occurred accessing the device hardware.

--*/

{

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EfiPlatformInitializeTimers (
    UINT32 *ClockTimerInterruptNumber,
    EFI_PLATFORM_SERVICE_TIMER_INTERRUPT *ClockTimerServiceRoutine,
    EFI_PLATFORM_READ_TIMER *ReadTimerRoutine,
    UINT64 *ReadTimerFrequency,
    UINT32 *ReadTimerWidth
    )

/*++

Routine Description:

    This routine initializes platform timer services. There are actually two
    different timer services returned in this routine. The periodic timer tick
    provides a periodic interrupt. The read timer provides a free running
    counter value. These are likely serviced by different timers. For the
    periodic timer tick, this routine should start the periodic interrupts
    coming in. The periodic rate of the timer can be anything reasonable, as
    the time counter will be used to count actual duration. The rate should be
    greater than twice the rollover rate of the time counter to ensure proper
    time accounting. Interrupts are disabled at the processor core for the
    duration of this routine.

Arguments:

    ClockTimerInterruptNumber - Supplies a pointer where the interrupt line
        number of the periodic timer tick will be returned.

    ClockTimerServiceRoutine - Supplies a pointer where a pointer to a routine
        called when the periodic timer tick interrupt occurs will be returned.

    ReadTimerRoutine - Supplies a pointer where a pointer to a routine
        called to read the current timer value will be returned.

    ReadTimerFrequency - Supplies the frequency of the counter.

    ReadTimerWidth - Supplies a pointer where the read timer bit width will be
        returned.

Return Value:

    EFI Status code.

--*/

{

    UINTN Index;
    EFI_STATUS Status;

    *ClockTimerInterruptNumber = INTEGRATOR_IRQ_TIMER1;
    *ClockTimerServiceRoutine = EfipPlatformServiceTimerInterrupt;
    *ReadTimerRoutine = EfipPlatformReadTimer;
    *ReadTimerFrequency = INTEGRATOR_TIMER_FREQUENCY;
    *ReadTimerWidth = 32;

    //
    // Use the two timers that run at a known frequency for the clock and
    // time counter.
    //

    Index = 1;
    EfiIntegratorClockTimer.BaseAddress =
        (VOID *)(INTEGRATOR_TIMER_BASE + (Index * IntegratorTimerRegisterSize));

    EfiIntegratorClockTimer.Index = Index;
    Index = 2;
    EfiIntegratorTimeCounter.BaseAddress =
        (VOID *)(INTEGRATOR_TIMER_BASE + (Index * IntegratorTimerRegisterSize));

    EfiIntegratorTimeCounter.Index = Index;

    //
    // Use GP timer 2 for the clock timer and GP timer 3 for the time counter.
    // Both run at 32kHz.
    //

    EfipIntegratorTimerInitialize(&EfiIntegratorClockTimer);
    EfipIntegratorTimerArm(&EfiIntegratorClockTimer,
                           TRUE,
                           INTEGRATOR_CLOCK_TICK_COUNT);

    EfipIntegratorTimerInitialize(&EfiIntegratorTimeCounter);
    Status = EfipPlatformSetInterruptLineState(*ClockTimerInterruptNumber,
                                               TRUE,
                                               FALSE);

    return Status;
}

VOID
EfiPlatformTerminateTimers (
    VOID
    )

/*++

Routine Description:

    This routine terminates timer services in preparation for the termination
    of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfipIntegratorTimerDisarm(&EfiIntegratorClockTimer);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipPlatformServiceTimerInterrupt (
    UINT32 InterruptNumber
    )

/*++

Routine Description:

    This routine is called to acknowledge a platform timer interrupt. This
    routine is responsible for quiescing the interrupt.

Arguments:

    InterruptNumber - Supplies the interrupt number that occurred.

Return Value:

    None.

--*/

{

    EfipIntegratorTimerAcknowledgeInterrupt(&EfiIntegratorClockTimer);
    return;
}

UINT64
EfipPlatformReadTimer (
    VOID
    )

/*++

Routine Description:

    This routine is called to read the current platform time value. The timer
    is assumed to be free running at a constant frequency, and should have a
    bit width as reported in the initialize function. The UEFI core will
    manage software bit extension out to 64 bits, this routine should just
    reporte the hardware timer value.

Arguments:

    None.

Return Value:

    Returns the hardware timer value.

--*/

{

    return EfipIntegratorTimerRead(&EfiIntegratorTimeCounter);
}

VOID
EfipIntegratorTimerInitialize (
    PINTEGRATOR_TIMER_DATA Timer
    )

/*++

Routine Description:

    This routine initializes an Integrator/CP timer.

Arguments:

    Timer - Supplies the pointer to the timer data.

Return Value:

    None.

--*/

{

    UINT32 ControlValue;

    //
    // Program the timer in free running mode with no interrupt generation.
    //

    ControlValue = INTEGRATOR_TIMER_ENABLED | INTEGRATOR_TIMER_DIVIDE_BY_1 |
                   INTEGRATOR_TIMER_32_BIT | INTEGRATOR_TIMER_MODE_FREE_RUNNING;

    WRITE_TIMER_REGISTER(Timer, IntegratorTimerControl, ControlValue);
    WRITE_TIMER_REGISTER(Timer, IntegratorTimerInterruptClear, 1);
    return;
}

UINT64
EfipIntegratorTimerRead (
    PINTEGRATOR_TIMER_DATA Timer
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

    Value = 0 - READ_TIMER_REGISTER(Timer, IntegratorTimerCurrentValue);
    return Value;
}

VOID
EfipIntegratorTimerArm (
    PINTEGRATOR_TIMER_DATA Timer,
    BOOLEAN Periodic,
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

    //
    // Set up the control value to program.
    //

    ControlValue = INTEGRATOR_TIMER_ENABLED | INTEGRATOR_TIMER_DIVIDE_BY_1 |
                   INTEGRATOR_TIMER_32_BIT | INTEGRATOR_TIMER_INTERRUPT_ENABLE;

    if (Periodic != FALSE) {
        ControlValue |= INTEGRATOR_TIMER_MODE_PERIODIC;

    } else {
        ControlValue |= INTEGRATOR_TIMER_MODE_ONE_SHOT;
    }

    //
    // Set the timer to its maximum value, set the configuration, clear the
    // interrupt, then set the value.
    //

    WRITE_TIMER_REGISTER(Timer, IntegratorTimerLoadValue, 0xFFFFFFFF);
    WRITE_TIMER_REGISTER(Timer, IntegratorTimerControl, ControlValue);
    WRITE_TIMER_REGISTER(Timer, IntegratorTimerInterruptClear, 1);
    WRITE_TIMER_REGISTER(Timer, IntegratorTimerLoadValue, TickCount);
    return;
}

VOID
EfipIntegratorTimerDisarm (
    PINTEGRATOR_TIMER_DATA Timer
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

    //
    // Disable the timer by programming it in free running mode with no
    // interrupt generation.
    //

    ControlValue = INTEGRATOR_TIMER_ENABLED | INTEGRATOR_TIMER_DIVIDE_BY_1 |
                   INTEGRATOR_TIMER_32_BIT | INTEGRATOR_TIMER_MODE_FREE_RUNNING;

    WRITE_TIMER_REGISTER(Timer, IntegratorTimerControl, ControlValue);
    WRITE_TIMER_REGISTER(Timer, IntegratorTimerInterruptClear, 1);
    return;
}

VOID
EfipIntegratorTimerAcknowledgeInterrupt (
    PINTEGRATOR_TIMER_DATA Timer
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

    WRITE_TIMER_REGISTER(Timer, IntegratorTimerInterruptClear, 1);
    return;
}


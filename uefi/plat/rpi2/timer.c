/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements platform timer services for the Raspberry Pi 2.

Author:

    Chris Stevens 19-Mar-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "rpi2fw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

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

//
// -------------------------------------------------------------------- Globals
//

BCM2709_TIMER EfiBcm2709ClockTimer;
BCM2709_TIMER EfiBcm2709TimeCounter;

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

    UINT32 Predivider;
    EFI_STATUS Status;

    //
    // Determine the frequency based on the APB clock frequency. The formula
    // is "ARM Timer Frequency" = ("APB Clock Frequency") / (Predivider + 1).
    // The Raspberry Pi 2's APB clock frequency is fixed and can achieve the
    // desired frequency of 1MHz given the defined predivider.
    //

    *ClockTimerInterruptNumber = BCM2709_CLOCK_TIMER_INTERRUPT;
    *ClockTimerServiceRoutine = EfipPlatformServiceTimerInterrupt;
    Predivider = RASPBERRY_PI_2_BCM2836_TIMER_PREDIVIDER_VALUE;

    //
    // The read timer uses the BCM2709's System Timer that runs at 1MHz.
    //

    *ReadTimerRoutine = EfipPlatformReadTimer;
    *ReadTimerFrequency = BCM2709_SYSTEM_TIMER_FREQUENCY;
    *ReadTimerWidth = 32;

    //
    // Use the two timers that run at a known frequency for the clock and
    // time counter.
    //

    EfiBcm2709ClockTimer.ClockTimer = TRUE;
    EfiBcm2709ClockTimer.Predivider = Predivider;
    EfiBcm2709TimeCounter.ClockTimer = FALSE;
    EfiBcm2709TimeCounter.Predivider = 0;

    //
    // Initialize the clock timer for periodic use.
    //

    Status = EfipBcm2709TimerInitialize(&EfiBcm2709ClockTimer);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfipBcm2709TimerArm(&EfiBcm2709ClockTimer, BCM2709_CLOCK_TICK_COUNT);
    Status = EfipBcm2709TimerInitialize(&EfiBcm2709TimeCounter);
    if (EFI_ERROR(Status)) {
        return Status;
    }

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

    EfipBcm2709TimerDisarm(&EfiBcm2709ClockTimer);
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

    EfipBcm2709TimerAcknowledgeInterrupt(&EfiBcm2709ClockTimer);
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

    return EfipBcm2709TimerRead(&EfiBcm2709TimeCounter);
}


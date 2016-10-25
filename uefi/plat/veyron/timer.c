/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements platform timer services for the RK3288 SoC.

Author:

    Evan Green 9-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "veyronfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an RK32xx timer. _Base should be a pointer, and
// _Register should be a RK32_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    EfiReadRegister32((_Base) + (_Register))

//
// This macro writes to an RK32xx timer. _Base should be a pointer,
// _Register should be RK32_TIMER_REGISTER value, and _Value should be a 32-bit
// integer.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    EfiWriteRegister32((_Base) + (_Register), (_Value))

//
// These macros read from and write to the watchdog timer.
//

#define RK32_READ_WATCHDOG(_Register) \
    EfiReadRegister32((VOID *)RK32_WATCHDOG_BASE + (_Register))

#define RK32_WRITE_WATCHDOG(_Register, _Value) \
    EfiWriteRegister32((VOID *)RK32_WATCHDOG_BASE + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of 24MHz clock ticks per interrupt. Shoot for 64
// interrupts per second.
//

#define VEYRON_TIMER_TICK_COUNT (RK32_TIMER_FREQUENCY / 64)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal state associated with an RK32xx timer.

Members:

    Base - Stores the virtual address of the timer.

    CountDown - Stores a boolean indicating whether the timer counts down
        (TRUE) or up (FALSE).

--*/

typedef struct _RK32_TIMER_DATA {
    VOID *Base;
    BOOLEAN CountDown;
} RK32_TIMER_DATA, *PRK32_TIMER_DATA;

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
EfipRk32TimerInitialize (
    PRK32_TIMER_DATA Context
    );

UINT64
EfipRk32TimerRead (
    PRK32_TIMER_DATA Context
    );

VOID
EfipRk32TimerArm (
    PRK32_TIMER_DATA Context,
    BOOLEAN Periodic,
    UINT64 TickCount
    );

VOID
EfipRk32TimerDisarm (
    PRK32_TIMER_DATA Context
    );

VOID
EfipRk32TimerAcknowledgeInterrupt (
    PRK32_TIMER_DATA Context
    );

EFI_STATUS
EfipRk32QueryApbAlivePclkFrequency (
    UINT32 *Frequency
    );

//
// -------------------------------------------------------------------- Globals
//

RK32_TIMER_DATA EfiVeyronClockTimer;
RK32_TIMER_DATA EfiVeyronTimeCounter;

//
// The watchdog timer runs on the APB Alive APB PCLK, whose frequency is
// calculated from the general PLL.
//

UINT32 EfiRk32ApbAlivePclkFrequency = 0;

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

    UINT32 Control;
    UINT32 CurrentCount;
    UINT64 DesiredCount;
    UINT32 Frequency;
    UINT32 RangeIndex;
    EFI_STATUS Status;

    //
    // Query the APB Alive PCLK frequency if necessary.
    //

    if (EfiRk32ApbAlivePclkFrequency == 0) {
        Status = EfipRk32QueryApbAlivePclkFrequency(&Frequency);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        EfiRk32ApbAlivePclkFrequency = Frequency;
    }

    DesiredCount = (Timeout * EfiRk32ApbAlivePclkFrequency);
    if (DesiredCount > RK32_WATCHDOG_MAX) {
        DesiredCount = RK32_WATCHDOG_MAX;
    }

    //
    // First, disable the watchdog timer.
    //

    Control = RK32_READ_WATCHDOG(Rk32WatchdogControl);
    Control &= ~RK32_WATCHDOG_CONTROL_ENABLE;
    RK32_WRITE_WATCHDOG(Rk32WatchdogControl, Control);

    //
    // If the watchdog timer is being enabled, set the count value and fire it
    // back up.
    //

    if ((DesiredCount != 0) && (EfiDisableWatchdog == FALSE)) {

        //
        // Figure out the proper range index for the requested count. The
        // allowable ranges go 0x0000FFFF, 0x0001FFFF, 0x0003FFFF, 0x0007FFFF,
        // 0x000FFFFF, etc all the way up to 0x7FFFFFFF.
        //

        RangeIndex = 0;
        CurrentCount = RK32_WATCHDOG_MIN;
        while (CurrentCount < DesiredCount) {
            RangeIndex += 1;
            CurrentCount = (CurrentCount << 1) | 0x1;
        }

        RK32_WRITE_WATCHDOG(Rk32WatchdogTimeoutRange, RangeIndex);

        //
        // Restart the counter. The TRM cruelly refers to this as "kicking the
        // dog".
        //

        RK32_WRITE_WATCHDOG(Rk32WatchdogCounterRestart,
                            RK32_WATCHDOG_RESTART_VALUE);

        //
        // Enable the watchdog.
        //

        Control |= RK32_WATCHDOG_CONTROL_ENABLE;
        Control &= ~RK32_WATCHDOG_CONTROL_BARK_FIRST;
        RK32_WRITE_WATCHDOG(Rk32WatchdogControl, Control);
    }

    return EFI_SUCCESS;
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

    EFI_STATUS Status;

    *ClockTimerInterruptNumber = RK32_INTERRUPT_TIMER0;
    *ClockTimerServiceRoutine = EfipPlatformServiceTimerInterrupt;
    *ReadTimerRoutine = EfipPlatformReadTimer;
    *ReadTimerFrequency = RK32_TIMER_FREQUENCY;
    *ReadTimerWidth = 64;

    //
    // Use timer 0 for the clock timer and timer 1 for the time counter. Both
    // run at 24MHz, and both count down.
    //

    EfiVeyronClockTimer.Base = (VOID *)(RK32_TIMER0_5_BASE +
                                        (0 * RK32_TIMER_REGISTER_STRIDE));

    EfiVeyronClockTimer.CountDown = TRUE;
    EfiVeyronTimeCounter.Base = (VOID *)(RK32_TIMER0_5_BASE +
                                         (1 * RK32_TIMER_REGISTER_STRIDE));

    EfiVeyronTimeCounter.CountDown = TRUE;
    EfipRk32TimerInitialize(&EfiVeyronClockTimer);
    EfipRk32TimerArm(&EfiVeyronClockTimer, TRUE, VEYRON_TIMER_TICK_COUNT);
    EfipRk32TimerInitialize(&EfiVeyronTimeCounter);
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

    EfipRk32TimerDisarm(&EfiVeyronClockTimer);
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

    EfipRk32TimerAcknowledgeInterrupt(&EfiVeyronClockTimer);
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

    return EfipRk32TimerRead(&EfiVeyronTimeCounter);
}

VOID
EfipRk32TimerInitialize (
    PRK32_TIMER_DATA Context
    )

/*++

Routine Description:

    This routine initializes an RK32xx timer.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    None.

--*/

{

    if (Context->Base == NULL) {
        return;
    }

    //
    // Program the timer in free running mode with no interrupt.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Rk32TimerControl,
                         RK32_TIMER_CONTROL_ENABLE);

    //
    // Set the load count register to the maximum period.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Rk32TimerLoadCountHigh,
                         0xFFFFFFFF);

    WRITE_TIMER_REGISTER(Context->Base,
                         Rk32TimerLoadCountLow,
                         0xFFFFFFFF);

    //
    // Clear any previously pending interrupts.
    //

    WRITE_TIMER_REGISTER(Context->Base, Rk32TimerInterruptStatus, 1);
    return;
}

UINT64
EfipRk32TimerRead (
    PRK32_TIMER_DATA Context
    )

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    Returns the timer's current count.

--*/

{

    UINT32 High1;
    UINT32 High2;
    UINT32 Low;
    UINT64 Value;

    //
    // Do a high-low-high read to make sure sure the words didn't tear.
    //

    do {
        High1 = READ_TIMER_REGISTER(Context->Base, Rk32TimerCurrentValueHigh);
        Low = READ_TIMER_REGISTER(Context->Base, Rk32TimerCurrentValueLow);
        High2 = READ_TIMER_REGISTER(Context->Base, Rk32TimerCurrentValueHigh);

    } while (High1 != High2);

    Value = (((UINT64)High1) << 32) | Low;
    if (Context->CountDown != FALSE) {
        Value = ~Value;
    }

    return Value;
}

VOID
EfipRk32TimerArm (
    PRK32_TIMER_DATA Context,
    BOOLEAN Periodic,
    UINT64 TickCount
    )

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Context - Supplies the pointer to the timer's context.

    Periodic - Supplies a boolean indicating if the timer should be armed
        periodically or one-shot.

    TickCount - Supplies the interval, in ticks, from now for the timer to fire
        in.

Return Value:

    None.

--*/

{

    UINT32 Control;

    if (Context->CountDown == FALSE) {
        TickCount = 0 - TickCount;
    }

    //
    // Stop the timer before programming it, as demanded by the TRM.
    //

    WRITE_TIMER_REGISTER(Context->Base, Rk32TimerControl, 0);

    //
    // Program the new tick count.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Rk32TimerLoadCountHigh,
                         TickCount >> 32);

    WRITE_TIMER_REGISTER(Context->Base, Rk32TimerLoadCountLow, TickCount);
    Control = RK32_TIMER_CONTROL_ENABLE | RK32_TIMER_CONTROL_INTERRUPT_ENABLE;
    if (Periodic == FALSE) {
        Control |= RK32_TIMER_CONTROL_ONE_SHOT;
    }

    WRITE_TIMER_REGISTER(Context->Base, Rk32TimerControl, Control);
    return;
}

VOID
EfipRk32TimerDisarm (
    PRK32_TIMER_DATA Context
    )

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    None.

--*/

{

    //
    // Just stop the timer completely.
    //

    WRITE_TIMER_REGISTER(Context->Base, Rk32TimerControl, 0);
    return;
}

VOID
EfipRk32TimerAcknowledgeInterrupt (
    PRK32_TIMER_DATA Context
    )

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    None.

--*/

{

    WRITE_TIMER_REGISTER(Context->Base, Rk32TimerInterruptStatus, 1);
    return;
}

EFI_STATUS
EfipRk32QueryApbAlivePclkFrequency (
    UINT32 *Frequency
    )

/*++

Routine Description:

    This routine queries the APB Alive PCLK frequency.

Arguments:

    Frequency - Supplies a pointer that receives the current frequency.

Return Value:

    Status code.

--*/

{

    UINT32 Divisor;
    UINT32 GeneralPllFrequency;
    EFI_STATUS Status;
    UINT32 Value;

    //
    // The APB Alive PCLK timer is taken from the General PLL and divided by
    // the value stored in clock select register 33.
    //

    Status = EfipRk32GetPllClockFrequency(Rk32PllGeneral, &GeneralPllFrequency);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Value = EfiReadRegister32((VOID *)RK32_CRU_BASE + Rk32CruClockSelect33);
    Divisor = (Value & RK32_CRU_CLOCK_SELECT33_ALIVE_PCLK_DIVIDER_MASK) >>
              RK32_CRU_CLOCK_SELECT33_ALIVE_PCLK_DIVIDER_SHIFT;

    *Frequency = GeneralPllFrequency / (Divisor + 1);
    return EFI_SUCCESS;
}


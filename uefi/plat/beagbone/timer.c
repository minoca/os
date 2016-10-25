/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements platform timer services for the TI AM335x. These
    timers function identically to the OMAP4 timers, and could be merged.

Author:

    Evan Green 19-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "bbonefw.h"
#include <minoca/soc/am335x.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an AM335 timer. _Base should be a pointer, and
// _Register should be a AM335_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    EfiReadRegister32((VOID *)(_Base) + (_Register))

//
// This macro writes to an AM335 timer. _Base should be a pointer,
// _Register should be GP_TIMER_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    EfiWriteRegister32((VOID *)(_Base) + (_Register), (_Value))

//
// These macros read from and write to the watchdog timer.
//

#define AM335_READ_WATCHDOG(_Register) \
    EfiReadRegister32((VOID *)AM335_WATCHDOG_BASE + (_Register))

#define AM335_WRITE_WATCHDOG(_Register, _Value) \
    EfiWriteRegister32((VOID *)AM335_WATCHDOG_BASE + (_Register), (_Value))

//
// These macros read from and write to the RTC.
//

#define AM3_READ_RTC(_Register) \
        *(volatile UINT32 *)(AM335_RTC_BASE + (_Register))

#define AM3_WRITE_RTC(_Register, _Value) \
        *((volatile UINT32 *)(AM335_RTC_BASE + (_Register))) = (_Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal state associated with an AM335 DM
    timer.

Members:

    Base - Stores the virtual address of the timer.

    Index - Stores the zero-based index of this timer within the timer block.

--*/

typedef struct _AM335_TIMER_DATA {
    VOID *Base;
    UINT32 Index;
    UINT32 Offset;
} AM335_TIMER_DATA, *PAM335_TIMER_DATA;

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
EfipAm335TimerInitialize (
    PAM335_TIMER_DATA Context
    );

UINT64
EfipAm335TimerRead (
    PAM335_TIMER_DATA Context
    );

VOID
EfipAm335TimerArm (
    PAM335_TIMER_DATA Context,
    BOOLEAN Periodic,
    UINT64 TickCount
    );

VOID
EfipAm335TimerDisarm (
    PAM335_TIMER_DATA Context
    );

VOID
EfipAm335TimerAcknowledgeInterrupt (
    PAM335_TIMER_DATA Context
    );

//
// -------------------------------------------------------------------- Globals
//

AM335_TIMER_DATA EfiBeagleBoneClockTimer;
AM335_TIMER_DATA EfiBeagleBoneTimeCounter;

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

    UINT32 Count;

    Count = 0 - (Timeout * AM335_WATCHDOG_FREQUENCY);

    //
    // First, disable the watchdog timer.
    //

    AM335_WRITE_WATCHDOG(Am335WatchdogStartStop, AM335_WATCHDOG_DISABLE1);
    EfiStall(1000);
    AM335_WRITE_WATCHDOG(Am335WatchdogStartStop, AM335_WATCHDOG_DISABLE2);
    EfiStall(1000);

    //
    // If the watchdog timer is being enabled, set the count value and fire it
    // back up.
    //

    if ((Count != 0) && (EfiDisableWatchdog == FALSE)) {
        AM335_WRITE_WATCHDOG(Am335WatchdogLoadCount, Count);
        EfiStall(1000);
        AM335_WRITE_WATCHDOG(Am335WatchdogCurrentCount, Count);
        EfiStall(1000);
        AM335_WRITE_WATCHDOG(Am335WatchdogStartStop, AM335_WATCHDOG_ENABLE1);
        EfiStall(1000);
        AM335_WRITE_WATCHDOG(Am335WatchdogStartStop, AM335_WATCHDOG_ENABLE2);
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

    *ClockTimerInterruptNumber = AM335_IRQ_DMTIMER0;
    *ClockTimerServiceRoutine = EfipPlatformServiceTimerInterrupt;
    *ReadTimerRoutine = EfipPlatformReadTimer;
    *ReadTimerFrequency = AM335_32KHZ_FREQUENCY;
    *ReadTimerWidth = 32;

    //
    // Use GP timer 0 for the clock timer and GP timer 2 for the time counter.
    // Both run at 32kHz.
    //

    EfiBeagleBoneClockTimer.Base = (VOID *)AM335_DMTIMER0_BASE;
    EfiBeagleBoneClockTimer.Index = 0;
    EfiBeagleBoneTimeCounter.Base = (VOID *)AM335_DMTIMER2_BASE;
    EfiBeagleBoneTimeCounter.Index = 2;
    EfipAm335TimerInitialize(&EfiBeagleBoneClockTimer);
    EfipAm335TimerArm(&EfiBeagleBoneClockTimer,
                      TRUE,
                      BEAGLEBONE_TIMER_TICK_COUNT);

    EfipAm335TimerInitialize(&EfiBeagleBoneTimeCounter);
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

    EfipAm335TimerDisarm(&EfiBeagleBoneClockTimer);
    return;
}

VOID
EfipBeagleBoneBlackInitializeRtc (
    VOID
    )

/*++

Routine Description:

    This routine fires up the RTC in the AM335x for the BeagleBone Black, if it
    is not already running.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Control;
    UINT32 Status;
    UINT32 Value;

    //
    // Set the RTC to smart idle wakeup-capable.
    //

    Value = AM335_RTC_SYS_CONFIG_IDLE_MODE_SMART_WAKEUP;
    AM3_WRITE_RTC(Am335RtcSysConfig, Value);

    //
    // If the RTC is already running, then it's been set up from a previous
    // boot.
    //

    Status = AM3_READ_RTC(Am335RtcStatus);
    if ((Status & AM335_RTC_STATUS_RUN) != 0) {
        goto BeagleBoneBlackInitializeRtcEnd;
    }

    //
    // If the RTC has been disabled by a previous boot, leave it alone, as the
    // spec seems to indicate there's no turning it back on once it's off.
    //

    Control = AM3_READ_RTC(Am335RtcControl);
    if ((Control & AM335_RTC_CONTROL_RTC_DISABLE) != 0) {
        return;
    }

    //
    // Unlock the RTC to program it.
    //

    AM3_WRITE_RTC(Am335RtcKick0, AM335_RTC_KICK0_KEY);
    AM3_WRITE_RTC(Am335RtcKick1, AM335_RTC_KICK1_KEY);

    //
    // Select the internal clock source, and enable inputs.
    //

    Value = AM3_READ_RTC(Am335RtcOscillator);
    Value &= ~AM335_RTC_OSCILLATOR_SOURCE_EXTERNAL;
    AM3_WRITE_RTC(Am335RtcOscillator, Value);
    Value |= AM335_RTC_OSCILLATOR_ENABLE;
    AM3_WRITE_RTC(Am335RtcOscillator, Value);

    //
    // Start the RTC running in 24 hour mode.
    //

    Value = AM335_RTC_CONTROL_RUN;
    AM3_WRITE_RTC(Am335RtcControl, Value);
    do {
        Value = AM3_READ_RTC(Am335RtcStatus);

    } while ((Value & AM335_RTC_STATUS_RUN) == 0);

    //
    // Lock the RTC to prevent accidental writes.
    //

    AM3_WRITE_RTC(Am335RtcKick0, AM335_RTC_KICK0_KEY);
    AM3_WRITE_RTC(Am335RtcKick1, 0xFFFFFFFF);

BeagleBoneBlackInitializeRtcEnd:
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

    EfipAm335TimerAcknowledgeInterrupt(&EfiBeagleBoneClockTimer);
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

    return EfipAm335TimerRead(&EfiBeagleBoneTimeCounter);
}

VOID
EfipAm335TimerInitialize (
    PAM335_TIMER_DATA Context
    )

/*++

Routine Description:

    This routine initializes an AM335 timer.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    None.

--*/

{

    UINT32 Value;

    if (Context->Base == NULL) {
        return;
    }

    //
    // Program the timer in free running mode with no interrupt.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerOcpConfig,
                         AM335_TIMER_IDLEMODE_SMART);

    //
    // Disable wakeup functionality.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerInterruptWakeEnable,
                         0);

    //
    // Set the synchronous interface configuration register to non-posted mode,
    // which means that writes don't return until they complete. Posted mode
    // is faster for writes but requires polling a bit for reads.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerSynchronousInterfaceControl,
                         0);

    //
    // Disable all interrupts for now. The alternate register interface uses a
    // set/clear style for the interrupt mask bits.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerInterruptEnableClear,
                         AM335_TIMER_INTERRUPT_MASK);

    //
    // Set the load value to zero to create a free-running timer, and reset the
    // current counter now too.
    //

    WRITE_TIMER_REGISTER(Context->Base, Am335TimerLoad, 0);
    WRITE_TIMER_REGISTER(Context->Base, Am335TimerCount, 0);

    //
    // Set the mode register to auto-reload, and start the timer.
    //

    Value = AM335_TIMER_OVERFLOW_TRIGGER | AM335_TIMER_STARTED |
            AM335_TIMER_AUTORELOAD;

    WRITE_TIMER_REGISTER(Context->Base, Am335TimerControl, Value);

    //
    // Reset all interrupt-pending bits.
    //
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerInterruptStatus,
                         AM335_TIMER_INTERRUPT_MASK);

    return;
}

UINT64
EfipAm335TimerRead (
    PAM335_TIMER_DATA Context
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

    UINT32 Value;

    Value = READ_TIMER_REGISTER(Context->Base, Am335TimerCount);
    return Value;
}

VOID
EfipAm335TimerArm (
    PAM335_TIMER_DATA Context,
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

    UINT32 Value;

    if (TickCount >= 0xFFFFFFFF) {
        TickCount = 0xFFFFFFFF;
    }

    //
    // Start the timer ticking.
    //

    WRITE_TIMER_REGISTER(Context->Base, Am335TimerControl, 0);
    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerLoad,
                         0xFFFFFFFF - (UINT32)TickCount);

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerCount,
                         0xFFFFFFFF - (UINT32)TickCount);

    Value = AM335_TIMER_STARTED;
    if (Periodic != FALSE) {
        Value |= AM335_TIMER_AUTORELOAD;
    }

    WRITE_TIMER_REGISTER(Context->Base, Am335TimerControl, Value);
    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerInterruptEnableSet,
                         AM335_TIMER_OVERFLOW_INTERRUPT);

    return;
}

VOID
EfipAm335TimerDisarm (
    PAM335_TIMER_DATA Context
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
    // Disable all interrupts.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerInterruptEnableClear,
                         AM335_TIMER_INTERRUPT_MASK);

    //
    // Reset all interrupt-pending bits.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerInterruptStatus,
                         AM335_TIMER_INTERRUPT_MASK);

    return;
}

VOID
EfipAm335TimerAcknowledgeInterrupt (
    PAM335_TIMER_DATA Context
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

    //
    // Clear the overflow interrupt by writing a 1 to the status bit.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         Am335TimerInterruptStatus,
                         AM335_TIMER_OVERFLOW_INTERRUPT);

    return;
}


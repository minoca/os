/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements platform timer services for the TI PandaBoard.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "pandafw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an OMAP4 timer. _Base should be a pointer, and
// _Register should be a GP_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    EfiReadRegister32((UINT32 *)(_Base) + (_Register))

//
// This macro writes to an OMAP4 timer. _Base should be a pointer,
// _Register should be GP_TIMER_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    EfiWriteRegister32((UINT32 *)(_Base) + (_Register), (_Value))

//
// These macros read from and write to the watchdog timer.
//

#define OMAP4_READ_WATCHDOG(_Register) \
    EfiReadRegister32((VOID *)OMAP4430_WATCHDOG2_BASE + (_Register))

#define OMAP4_WRITE_WATCHDOG(_Register, _Value) \
    EfiWriteRegister32((VOID *)OMAP4430_WATCHDOG2_BASE + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_WATCHDOG_FREQUENCY 32768

//
// Define the number of 32kHz clock ticks per interrupt. A value of 512 creates
// a timer rate of 15.625ms, or about 64 interrupts per second.
//

#define PANDA_BOARD_TIMER_TICK_COUNT 512

//
// Define the offset between the standard register offsets and the alternates.
//

#define OMAP4_TIMER_ALTERATE_REGISTER_OFFSET 5

//
// Idle bits.
//

#define GPTIMER_IDLEMODE_NOIDLE 0x00000080

//
// Mode bits.
//

#define GPTIMER_STARTED 0x00000001
#define GPTIMER_OVERFLOW_TRIGGER 0x00000400
#define GPTIMER_OVERFLOW_AND_MATCH_TRIGGER 0x00000800
#define GPTIMER_COMPARE_ENABLED 0x00000040
#define GPTIMER_AUTORELOAD 0x00000002

//
// Interrupt enable bits.
//

#define GPTIMER_MATCH_INTERRUPT 0x00000001
#define GPTIMER_OVERFLOW_INTERRUPT 0x00000002

//
// Define the two step sequence needed for disabling or enabling the watchdog
// timer.
//

#define OMAP4_WATCHDOG_DISABLE1 0x0000AAAA
#define OMAP4_WATCHDOG_DISABLE2 0x00005555
#define OMAP4_WATCHDOG_ENABLE1  0x0000BBBB
#define OMAP4_WATCHDOG_ENABLE2  0x00004444

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the GP timer register set, with offsets in UINT32s. This is a bit
// confusing because on the OMAP4 there are two different (but very similar)
// register sets depending on the timer. Starting with the Wakeup register
// they're simply off by a fixed offset. Before then, they're slightly
// different. The alternate registers (for GPTIMERs 3-9 and 11) are interleaved
// here with the standard ones. The values here have also already taken into
// account the fact that an offset is going to be added, so that alternate
// ones are 5 ULONGs shy of their actual register offsets (the fixed offset
// once things get back in sync).
//

typedef enum _GP_TIMER_REGISTER {
    GpTimerRevision                 = 0x00, // GPT_TIDR
    GpTimerInterfaceConfiguration1  = 0x04, // GPT1MS_TIOCP_CFG
    GpTimerRawInterruptStatus       = 0x04, // GPT_IRQSTATUS_RAW
    GpTimerStatus                   = 0x05, // GPT_TISTAT
    GpTimerInterruptStatusAlternate = 0x05, // GPT_IRQSTATUS
    GpTimerInterruptStatus          = 0x06, // GPT_TISR
    GpTimerInterruptEnableAlternate = 0x06, // GPT_IRQENABLE_SET
    GpTimerInterruptEnable          = 0x07, // GPT_TIER
    GpTimerInterruptDisable         = 0x07, // GPT_IRQENABLE_CLR
    GpTimerWakeup                   = 0x08, // GPT_TWER
    GpTimerMode                     = 0x09, // GPT_TCLR
    GpTimerCurrentCount             = 0x0A, // GPT_TCRR
    GpTimerLoadCount                = 0x0B, // GPT_TLDR
    GpTimerTriggerReload            = 0x0C, // GPT_TTGR
    GpTimerWritePending             = 0x0D, // GPT_TWPS
    GpTimerMatchCount               = 0x0E, // GPT_TMAR
    GpTimerCapture1                 = 0x0F, // GPT_TCAR1
    GpTimerInterfaceConfiguration2  = 0x10, // GPT_TSICR
    GpTimerCapture2                 = 0x11, // GPT_TCAR2
    GpTimerPositive1msIncrement     = 0x12, // GPT_TPIR
    GpTimerNegative1msIncrement     = 0x13, // GPT_TNIR
    GpTimerCurrentRounding1ms       = 0x14, // GPT_TCVR
    GpTimerOverflowValue            = 0x16, // GPT_TOCR
    GpTimerMaskedOverflowCount      = 0x17, // GPT_TOWR
} GP_TIMER_REGISTER, *PGP_TIMER_REGISTER;

//
// Define the watchdog timer registers, offsets in bytes.
//

typedef enum _OMAP4_WATCHDOG_REGISTER {
    Omap4WatchdogRevision               = 0x00,
    Omap4WatchdogInterfaceConfiguration = 0x10,
    Omap4WatchdogInterfaceStatus        = 0x14,
    Omap4WatchdogInterruptStatus        = 0x18,
    Omap4WatchdogInterruptEnable        = 0x1C,
    Omap4WatchdogWakeEventEnable        = 0x20,
    Omap4WatchdogPrescaler              = 0x24,
    Omap4WatchdogCurrentCount           = 0x28,
    Omap4WatchdogLoadCount              = 0x2C,
    Omap4WatchdogWritePostControl       = 0x34,
    Omap4WatchdogDelay                  = 0x44,
    Omap4WatchdogStartStop              = 0x48,
    Omap4WatchdogRawInterruptStatus     = 0x54,
    Omap4WatchdogInterruptEnableSet     = 0x5C,
    Omap4WatchdogInterruptEnableClear   = 0x60,
    Omap4WatchdogWakeEnable             = 0x64
} OMAP4_WATCHDOG_REGISTER, *POMAP4_WATCHDOG_REGISTER;

/*++

Structure Description:

    This structure stores the internal state associated with an OMAP4 GP
    timer.

Members:

    Base - Stores the virtual address of the timer.

    Index - Stores the zero-based index of this timer within the timer block.

    Offset - Stores the offset, in ULONGs, that should be applied to every
        register access because the timer is using the alternate register
        definitions.

--*/

typedef struct _GP_TIMER_DATA {
    UINT32 *Base;
    UINT32 Index;
    UINT32 Offset;
} GP_TIMER_DATA, *PGP_TIMER_DATA;

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
EfipOmap4TimerInitialize (
    PGP_TIMER_DATA Context
    );

UINT64
EfipOmap4TimerRead (
    PGP_TIMER_DATA Context
    );

VOID
EfipOmap4TimerArm (
    PGP_TIMER_DATA Context,
    BOOLEAN Periodic,
    UINT64 TickCount
    );

VOID
EfipOmap4TimerDisarm (
    PGP_TIMER_DATA Context
    );

VOID
EfipOmap4TimerAcknowledgeInterrupt (
    PGP_TIMER_DATA Context
    );

//
// -------------------------------------------------------------------- Globals
//

GP_TIMER_DATA EfiPandaClockTimer;
GP_TIMER_DATA EfiPandaTimeCounter;

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

    Count = 0 - (Timeout * OMAP4_WATCHDOG_FREQUENCY);

    //
    // First, disable the watchdog timer.
    //

    OMAP4_WRITE_WATCHDOG(Omap4WatchdogStartStop, OMAP4_WATCHDOG_DISABLE1);
    EfiStall(1000);
    OMAP4_WRITE_WATCHDOG(Omap4WatchdogStartStop, OMAP4_WATCHDOG_DISABLE2);
    EfiStall(1000);

    //
    // If the watchdog timer is being enabled, set the count value and fire it
    // back up.
    //

    if ((Count != 0) && (EfiDisableWatchdog == FALSE)) {
        OMAP4_WRITE_WATCHDOG(Omap4WatchdogLoadCount, Count);
        EfiStall(1000);
        OMAP4_WRITE_WATCHDOG(Omap4WatchdogCurrentCount, Count);
        EfiStall(1000);
        OMAP4_WRITE_WATCHDOG(Omap4WatchdogStartStop, OMAP4_WATCHDOG_ENABLE1);
        EfiStall(1000);
        OMAP4_WRITE_WATCHDOG(Omap4WatchdogStartStop, OMAP4_WATCHDOG_ENABLE2);
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

    *ClockTimerInterruptNumber = OMAP4430_IRQ_GPTIMER2;
    *ClockTimerServiceRoutine = EfipPlatformServiceTimerInterrupt;
    *ReadTimerRoutine = EfipPlatformReadTimer;
    *ReadTimerFrequency = OMAP4430_32KHZ_FREQUENCY;
    *ReadTimerWidth = 32;

    //
    // Use GP timer 2 for the clock timer and GP timer 3 for the time counter.
    // Both run at 32kHz.
    //

    EfiPandaClockTimer.Base = (VOID *)OMAP4430_GPTIMER2_BASE;
    EfiPandaClockTimer.Index = 1;
    EfiPandaClockTimer.Offset = 0;
    EfiPandaTimeCounter.Base = (VOID *)OMAP4430_GPTIMER3_BASE;
    EfiPandaTimeCounter.Index = 2;
    EfiPandaTimeCounter.Offset = OMAP4_TIMER_ALTERATE_REGISTER_OFFSET;
    EfipOmap4TimerInitialize(&EfiPandaClockTimer);
    EfipOmap4TimerArm(&EfiPandaClockTimer, TRUE, PANDA_BOARD_TIMER_TICK_COUNT);
    EfipOmap4TimerInitialize(&EfiPandaTimeCounter);
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

    EfipOmap4TimerDisarm(&EfiPandaClockTimer);
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

    EfipOmap4TimerAcknowledgeInterrupt(&EfiPandaClockTimer);
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

    return EfipOmap4TimerRead(&EfiPandaTimeCounter);
}

VOID
EfipOmap4TimerInitialize (
    PGP_TIMER_DATA Context
    )

/*++

Routine Description:

    This routine initializes an OMAP4 timer.

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
    // Program the timer in free running mode with no interrupt. Set the
    // interface configuration to a state that disables going idle. This is
    // the only register that does not change at all between the standard
    // and alternate interface.
    //

    WRITE_TIMER_REGISTER(Context->Base,
                         GpTimerInterfaceConfiguration1,
                         GPTIMER_IDLEMODE_NOIDLE);

    //
    // Disable wakeup functionality.
    //

    WRITE_TIMER_REGISTER(Context->Base + Context->Offset, GpTimerWakeup, 0);

    //
    // Set the second interface configuration register to non-posted mode,
    // which means that writes don't return until they complete. Posted mode
    // is faster for writes but requires polling a bit for reads.
    //

    WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                         GpTimerInterfaceConfiguration2,
                         0);

    //
    // Disable all interrupts for now. The alternate register interface uses a
    // set/clear style for the interrupt mask bits.
    //

    if (Context->Offset == 0) {
        WRITE_TIMER_REGISTER(Context->Base, GpTimerInterruptEnable, 0);

    } else {
        WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                             GpTimerInterruptDisable,
                             0x7);
    }

    //
    // Set the load value to zero to create a free-running timer, and reset the
    // current counter now too.
    //

    WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                         GpTimerLoadCount,
                         0x00000000);

    WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                         GpTimerCurrentCount,
                         0x00000000);

    //
    // Set the mode register to auto-reload, and start the timer.
    //

    Value = GPTIMER_OVERFLOW_TRIGGER | GPTIMER_STARTED | GPTIMER_AUTORELOAD;
    WRITE_TIMER_REGISTER(Context->Base + Context->Offset, GpTimerMode, Value);

    //
    // Reset all interrupt-pending bits. This register has a unique offset
    // in the alternate interface.
    //

    if (Context->Offset == 0) {
        WRITE_TIMER_REGISTER(Context->Base, GpTimerInterruptStatus, 0x7);

    } else {
        WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                             GpTimerInterruptStatusAlternate,
                             0x7);
    }

    return;
}

UINT64
EfipOmap4TimerRead (
    PGP_TIMER_DATA Context
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

    Value = READ_TIMER_REGISTER(Context->Base + Context->Offset,
                                GpTimerCurrentCount);

    return Value;
}

VOID
EfipOmap4TimerArm (
    PGP_TIMER_DATA Context,
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

    WRITE_TIMER_REGISTER(Context->Base + Context->Offset, GpTimerMode, 0);
    WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                         GpTimerLoadCount,
                         0xFFFFFFFF - (UINT32)TickCount);

    WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                         GpTimerCurrentCount,
                         0xFFFFFFFF - (UINT32)TickCount);

    Value = GPTIMER_STARTED;
    if (Periodic != FALSE) {
        Value |= GPTIMER_AUTORELOAD;
    }

    WRITE_TIMER_REGISTER(Context->Base + Context->Offset, GpTimerMode, Value);
    if (Context->Offset == 0) {
        WRITE_TIMER_REGISTER(Context->Base,
                             GpTimerInterruptEnable,
                             GPTIMER_OVERFLOW_INTERRUPT);

    } else {
        WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                             GpTimerInterruptEnableAlternate,
                             GPTIMER_OVERFLOW_INTERRUPT);
    }

    return;
}

VOID
EfipOmap4TimerDisarm (
    PGP_TIMER_DATA Context
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
    // Disable all interrupts. The alternate register interface uses a
    // set/clear style for the interrupt mask bits.
    //

    if (Context->Offset == 0) {
        WRITE_TIMER_REGISTER(Context->Base, GpTimerInterruptEnable, 0);

    } else {
        WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                             GpTimerInterruptDisable,
                             0x7);
    }

    //
    // Reset all interrupt-pending bits. This register has a unique offset
    // in the alternate interface.
    //

    if (Context->Offset == 0) {
        WRITE_TIMER_REGISTER(Context->Base, GpTimerInterruptStatus, 0x7);

    } else {
        WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                             GpTimerInterruptStatusAlternate,
                             0x7);
    }

    return;
}

VOID
EfipOmap4TimerAcknowledgeInterrupt (
    PGP_TIMER_DATA Context
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

    if (Context->Offset == 0) {
        WRITE_TIMER_REGISTER(Context->Base,
                             GpTimerInterruptStatus,
                             GPTIMER_OVERFLOW_INTERRUPT);

    } else {
        WRITE_TIMER_REGISTER(Context->Base + Context->Offset,
                             GpTimerInterruptStatusAlternate,
                             GPTIMER_OVERFLOW_INTERRUPT);
    }

    return;
}


/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap3tmr.c

Abstract:

    This module implements support for the GP Timers on the TI OMAP3.

Author:

    Evan Green 3-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Include kernel.h, but be cautious about which APIs are used. Most of the
// system depends on the hardware modules. Limit use to HL, RTL and AR routines.
//

#include <minoca/kernel/kernel.h>
#include "omap3.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an OMAP3 timer. _Base should be a pointer, and
// _Register should be a GP_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    HlReadRegister32((PULONG)(_Base) + (_Register))

//
// This macro writes to an OMAP3 timer. _Base should be a pointer,
// _Register should be GP_TIMER_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((PULONG)(_Base) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
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
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpOmap3TimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpOmap3TimerRead (
    PVOID Context
    );

VOID
HlpOmap3TimerWrite (
    PVOID Context,
    ULONGLONG NewCount
    );

KSTATUS
HlpOmap3TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpOmap3TimerDisarm (
    PVOID Context
    );

VOID
HlpOmap3TimerAcknowledgeInterrupt (
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the GP timer register set, with offsets in ULONGs.
//

typedef enum _GP_TIMER_REGISTER {
    GpTimerReserved0,
    GpTimerReserved1,
    GpTimerReserved2,
    GpTimerReserved3,
    GpTimerInterfaceConfiguration1, // TIOCP_CFG
    GpTimerStatus,                  // TISTAT
    GpTimerInterruptStatus,         // TISR
    GpTimerInterruptEnable,         // TIER
    GpTimerWakeup,                  // TWER
    GpTimerMode,                    // TCLR
    GpTimerCurrentCount,            // TCRR
    GpTimerLoadCount,               // TLDR
    GpTimerTriggerReload,           // TTGR
    GpTimerWritePending,            // TWPS
    GpTimerMatchCount,              // TMAR
    GpTimerCapture1,                // TCAR1
    GpTimerInterfaceConfiguration2, // TSICR
    GpTimerCapture2,                // TCAR2
    GpTimerPositive1msIncrement,    // TPIR
    GpTimerNegative1msIncrement,    // TNIR
    GpTimerCurrentRounding1ms,      // TCVR
    GpTimerOverflowValue,           // TOCR
    GpTimerMaskedOverflowCount,     // TOWR
} GP_TIMER_REGISTER, *PGP_TIMER_REGISTER;

/*++

Structure Description:

    This structure stores the internal state associated with an OMAP3 GP
    timer.

Members:

    Base - Stores the virtual address of the timer.

    PhysicalAddress - Stores the physical address of the timer.

    Index - Stores the zero-based index of this timer within the timer block.

--*/

typedef struct _GP_TIMER_DATA {
    PVOID Base;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Index;
} GP_TIMER_DATA, *PGP_TIMER_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the OMAP ACPI table.
//

POMAP3_TABLE HlOmap3Table = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpOmap3TimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the OMAP3 GP Timer hardware module.
    Its role is to detect and report the prescense of OMAP3 Timers.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    TIMER_DESCRIPTION Timer;
    PGP_TIMER_DATA TimerData;
    ULONG TimerIndex;

    //
    // Interrupt controllers are always initialized before timers, so the
    // OMAP3 ACPI table should already be set up.
    //

    if (HlOmap3Table == NULL) {
        goto GpTimerModuleEntryEnd;
    }

    //
    // Fire up the timer's power.
    //

    Status = HlpOmap3InitializePowerAndClocks();
    if (!KSUCCESS(Status)) {
        goto GpTimerModuleEntryEnd;
    }

    //
    // Register each of the independent timers in the timer block.
    //

    for (TimerIndex = 0; TimerIndex < OMAP3_TIMER_COUNT; TimerIndex += 1) {

        //
        // Skip the timer if it has no address.
        //

        if (HlOmap3Table->TimerPhysicalAddress[TimerIndex] == (UINTN)NULL) {
            continue;
        }

        RtlZeroMemory(&Timer, sizeof(TIMER_DESCRIPTION));
        Timer.TableVersion = TIMER_DESCRIPTION_VERSION;
        Timer.FunctionTable.Initialize = HlpOmap3TimerInitialize;
        Timer.FunctionTable.ReadCounter = HlpOmap3TimerRead;
        Timer.FunctionTable.WriteCounter = HlpOmap3TimerWrite;
        Timer.FunctionTable.Arm = HlpOmap3TimerArm;
        Timer.FunctionTable.Disarm = HlpOmap3TimerDisarm;
        Timer.FunctionTable.AcknowledgeInterrupt =
                                             HlpOmap3TimerAcknowledgeInterrupt;

        TimerData = HlAllocateMemory(sizeof(GP_TIMER_DATA),
                                     OMAP3_ALLOCATION_TAG,
                                     FALSE,
                                     NULL);

        if (TimerData == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GpTimerModuleEntryEnd;
        }

        RtlZeroMemory(TimerData, sizeof(GP_TIMER_DATA));
        TimerData->PhysicalAddress =
                                HlOmap3Table->TimerPhysicalAddress[TimerIndex];

        TimerData->Index = TimerIndex;
        Timer.Context = TimerData;
        Timer.Features = TIMER_FEATURE_READABLE |
                         TIMER_FEATURE_WRITABLE |
                         TIMER_FEATURE_PERIODIC |
                         TIMER_FEATURE_ONE_SHOT;

        Timer.CounterBitWidth = OMAP3_TIMER_BIT_WIDTH;

        //
        // The first timer runs at the bus clock speed, but the rest run at
        // a fixed frequency.
        //

        if (TimerIndex == 0) {
            Timer.CounterFrequency = 0;

        } else {
            Timer.CounterFrequency = OMAP3_TIMER_FIXED_FREQUENCY;
        }

        Timer.Interrupt.Line.Type = InterruptLineControllerSpecified;
        Timer.Interrupt.Line.U.Local.Controller = 0;
        Timer.Interrupt.Line.U.Local.Line = HlOmap3Table->TimerGsi[TimerIndex];
        Timer.Interrupt.TriggerMode = InterruptModeUnknown;
        Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;

        //
        // Register the timer with the system.
        //

        Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
        if (!KSUCCESS(Status)) {
            goto GpTimerModuleEntryEnd;
        }
    }

GpTimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpOmap3TimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an OMAP3 timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    PGP_TIMER_DATA Timer;
    ULONG Value;

    Timer = (PGP_TIMER_DATA)Context;

    //
    // Map the hardware if that has not been done.
    //

    if (Timer->Base == NULL) {
        Timer->Base = HlMapPhysicalAddress(Timer->PhysicalAddress,
                                           OMAP3_TIMER_CONTROLLER_SIZE,
                                           TRUE);

        if (Timer->Base == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GpTimerInitializeEnd;
        }
    }

    //
    // Program the timer in free running mode with no interrupt. Set the
    // interface configuration to a state that disables going idle.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         GpTimerInterfaceConfiguration1,
                         GPTIMER_IDLEMODE_NOIDLE);

    //
    // Disable wakeup functionality.
    //

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerWakeup, 0);

    //
    // Set the second interface configuration register to non-posted mode,
    // which means that writes don't return until they complete. Posted mode
    // is faster for writes but requires polling a bit for reads.
    //

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterfaceConfiguration2, 0);

    //
    // Disable all interrupts for now.
    //

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptEnable, 0);

    //
    // Set the load value to zero to create a free-running timer, and reset the
    // current counter now too.
    //

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerLoadCount, 0x00000000);
    WRITE_TIMER_REGISTER(Timer->Base, GpTimerCurrentCount, 0x00000000);

    //
    // Set the mode register to auto-reload, and start the timer.
    //

    Value = GPTIMER_OVERFLOW_TRIGGER | GPTIMER_STARTED | GPTIMER_AUTORELOAD;
    WRITE_TIMER_REGISTER(Timer->Base, GpTimerMode, Value);

    //
    // Reset all interrupt-pending bits.
    //

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptStatus, 0x7);
    Status = STATUS_SUCCESS;

GpTimerInitializeEnd:
    return Status;
}

ULONGLONG
HlpOmap3TimerRead (
    PVOID Context
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

    PGP_TIMER_DATA Timer;

    Timer = (PGP_TIMER_DATA)Context;
    return READ_TIMER_REGISTER(Timer->Base, GpTimerCurrentCount);
}

VOID
HlpOmap3TimerWrite (
    PVOID Context,
    ULONGLONG NewCount
    )

/*++

Routine Description:

    This routine writes to the timer's hardware counter. This routine will
    only be called for timers that have the writable counter feature bit set.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewCount - Supplies the value to write into the counter. It is expected that
        the counter will not stop after the write.

Return Value:

    None.

--*/

{

    PGP_TIMER_DATA Timer;

    Timer = (PGP_TIMER_DATA)Context;
    WRITE_TIMER_REGISTER(Timer->Base, GpTimerCurrentCount, (ULONG)NewCount);
    return;
}

KSTATUS
HlpOmap3TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    )

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    Mode - Supplies the mode to arm the timer in. The system will never request
        a mode not supported by the timer's feature bits. The mode dictates
        how the tick count argument is interpreted.

    TickCount - Supplies the number of timer ticks from now for the timer to
        fire in. In absolute mode, this supplies the time in timer ticks at
        which to fire an interrupt.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    PGP_TIMER_DATA Timer;
    ULONG Value;

    Timer = (PGP_TIMER_DATA)Context;
    if (TickCount >= MAX_ULONG) {
        TickCount = MAX_ULONG - 1;
    }

    //
    // Start the timer ticking.
    //

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerMode, 0);
    WRITE_TIMER_REGISTER(Timer->Base,
                         GpTimerLoadCount,
                         0xFFFFFFFF - (ULONG)TickCount);

    WRITE_TIMER_REGISTER(Timer->Base,
                         GpTimerCurrentCount,
                         0xFFFFFFFF - (ULONG)TickCount);

    Value = GPTIMER_STARTED;
    if (Mode == TimerModePeriodic) {
        Value |= GPTIMER_AUTORELOAD;
    }

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerMode, Value);
    WRITE_TIMER_REGISTER(Timer->Base,
                         GpTimerInterruptEnable,
                         GPTIMER_OVERFLOW_INTERRUPT);

    return STATUS_SUCCESS;
}

VOID
HlpOmap3TimerDisarm (
    PVOID Context
    )

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    PGP_TIMER_DATA Timer;

    //
    // Disable all interrupts on this timer.
    //

    Timer = (PGP_TIMER_DATA)Context;
    WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptEnable, 0);

    //
    // Reset all interrupt-pending bits.
    //

    WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptStatus, 0x7);
    return;
}

VOID
HlpOmap3TimerAcknowledgeInterrupt (
    PVOID Context
    )

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    PGP_TIMER_DATA Timer;

    Timer = (PGP_TIMER_DATA)Context;

    //
    // Clear the overflow interrupt by writing a 1 to the status bit.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         GpTimerInterruptStatus,
                         GPTIMER_OVERFLOW_INTERRUPT);

    return;
}


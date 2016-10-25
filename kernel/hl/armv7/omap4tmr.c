/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap4tmr.c

Abstract:

    This module implements support for the GP Timers on the TI OMAP4.

Author:

    Evan Green 4-Nov-2012

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
#include "omap4.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an OMAP4 timer. _Base should be a pointer, and
// _Register should be a GP_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    HlReadRegister32((PULONG)(_Base) + (_Register))

//
// This macro writes to an OMAP4 timer. _Base should be a pointer,
// _Register should be GP_TIMER_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((PULONG)(_Base) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

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
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpOmap4TimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpOmap4TimerRead (
    PVOID Context
    );

VOID
HlpOmap4TimerWrite (
    PVOID Context,
    ULONGLONG NewCount
    );

KSTATUS
HlpOmap4TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpOmap4TimerDisarm (
    PVOID Context
    );

VOID
HlpOmap4TimerAcknowledgeInterrupt (
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the GP timer register set, with offsets in ULONGs. This is a bit
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

/*++

Structure Description:

    This structure stores the internal state associated with an OMAP4 GP
    timer.

Members:

    Base - Stores the virtual address of the timer.

    PhysicalAddress - Stores the physical address of the timer.

    Index - Stores the zero-based index of this timer within the timer block.

    Offset - Stores the offset, in ULONGs, that should be applied to every
        register access because the timer is using the alternate register
        definitions.

--*/

typedef struct _GP_TIMER_DATA {
    PULONG Base;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Index;
    ULONG Offset;
} GP_TIMER_DATA, *PGP_TIMER_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the OMAP ACPI table.
//

POMAP4_TABLE HlOmap4Table = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpOmap4TimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the OMAP4 GP Timer hardware module.
    Its role is to detect and report the prescense of OMAP4 Timers.

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
    // Attempt to find the OMAP4 ACPI table. There is no OMAP4 interrupt
    // controller (the GIC is used) so the timer module needs to get the table.
    //

    HlOmap4Table = HlGetAcpiTable(OMAP4_SIGNATURE, NULL);
    if (HlOmap4Table == NULL) {
        goto GpTimerModuleEntryEnd;
    }

    //
    // Fire up the timer's power.
    //

    Status = HlpOmap4InitializePowerAndClocks();
    if (!KSUCCESS(Status)) {
        goto GpTimerModuleEntryEnd;
    }

    //
    // Register each of the independent timers in the timer block.
    //

    for (TimerIndex = 0; TimerIndex < OMAP4_TIMER_COUNT; TimerIndex += 1) {

        //
        // Skip the timer if it has no address.
        //

        if (HlOmap4Table->TimerPhysicalAddress[TimerIndex] == (UINTN)NULL) {
            continue;
        }

        RtlZeroMemory(&Timer, sizeof(TIMER_DESCRIPTION));
        Timer.TableVersion = TIMER_DESCRIPTION_VERSION;
        Timer.FunctionTable.Initialize = HlpOmap4TimerInitialize;
        Timer.FunctionTable.ReadCounter = HlpOmap4TimerRead;
        Timer.FunctionTable.WriteCounter = HlpOmap4TimerWrite;
        Timer.FunctionTable.Arm = HlpOmap4TimerArm;
        Timer.FunctionTable.Disarm = HlpOmap4TimerDisarm;
        Timer.FunctionTable.AcknowledgeInterrupt =
                                             HlpOmap4TimerAcknowledgeInterrupt;

        TimerData = HlAllocateMemory(sizeof(GP_TIMER_DATA),
                                     OMAP4_ALLOCATION_TAG,
                                     FALSE,
                                     NULL);

        if (TimerData == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GpTimerModuleEntryEnd;
        }

        RtlZeroMemory(TimerData, sizeof(GP_TIMER_DATA));
        TimerData->PhysicalAddress =
                                HlOmap4Table->TimerPhysicalAddress[TimerIndex];

        TimerData->Index = TimerIndex;
        if ((TimerIndex == 0) || (TimerIndex == 1) || (TimerIndex == 9)) {
            TimerData->Offset = 0;

        } else {
            TimerData->Offset = OMAP4_TIMER_ALTERATE_REGISTER_OFFSET;
        }

        Timer.Context = TimerData;
        Timer.Features = TIMER_FEATURE_READABLE |
                         TIMER_FEATURE_WRITABLE |
                         TIMER_FEATURE_PERIODIC |
                         TIMER_FEATURE_ONE_SHOT;

        Timer.CounterBitWidth = OMAP4_TIMER_BIT_WIDTH;

        //
        // The first timer runs at the bus clock speed, but the rest run at
        // a fixed frequency.
        //

        if (TimerIndex == 0) {
            Timer.CounterFrequency = 0;

        } else {
            Timer.CounterFrequency = OMAP4_TIMER_FIXED_FREQUENCY;
        }

        Timer.Interrupt.Line.Type = InterruptLineControllerSpecified;
        Timer.Interrupt.Line.U.Local.Controller = 0;
        Timer.Interrupt.Line.U.Local.Line = HlOmap4Table->TimerGsi[TimerIndex];
        Timer.Interrupt.TriggerMode = InterruptModeLevel;
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
HlpOmap4TimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an OMAP4 timer.

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
                                           OMAP4_TIMER_CONTROLLER_SIZE,
                                           TRUE);

        if (Timer->Base == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GpTimerInitializeEnd;
        }
    }

    //
    // Program the timer in free running mode with no interrupt. Set the
    // interface configuration to a state that disables going idle. This is
    // the only register that does not change at all between the standard
    // and alternate interface.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         GpTimerInterfaceConfiguration1,
                         GPTIMER_IDLEMODE_NOIDLE);

    //
    // Disable wakeup functionality.
    //

    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset, GpTimerWakeup, 0);

    //
    // Set the second interface configuration register to non-posted mode,
    // which means that writes don't return until they complete. Posted mode
    // is faster for writes but requires polling a bit for reads.
    //

    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                         GpTimerInterfaceConfiguration2,
                         0);

    //
    // Disable all interrupts for now. The alternate register interface uses a
    // set/clear style for the interrupt mask bits.
    //

    if (Timer->Offset == 0) {
        WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptEnable, 0);

    } else {
        WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                             GpTimerInterruptDisable,
                             0x7);
    }

    //
    // Set the load value to zero to create a free-running timer, and reset the
    // current counter now too.
    //

    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                         GpTimerLoadCount,
                         0x00000000);

    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                         GpTimerCurrentCount,
                         0x00000000);

    //
    // Set the mode register to auto-reload, and start the timer.
    //

    Value = GPTIMER_OVERFLOW_TRIGGER | GPTIMER_STARTED | GPTIMER_AUTORELOAD;
    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset, GpTimerMode, Value);

    //
    // Reset all interrupt-pending bits. This register has a unique offset
    // in the alternate interface.
    //

    if (Timer->Offset == 0) {
        WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptStatus, 0x7);

    } else {
        WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                             GpTimerInterruptStatusAlternate,
                             0x7);
    }

    Status = STATUS_SUCCESS;

GpTimerInitializeEnd:
    return Status;
}

ULONGLONG
HlpOmap4TimerRead (
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
    ULONG Value;

    Timer = (PGP_TIMER_DATA)Context;
    Value = READ_TIMER_REGISTER(Timer->Base + Timer->Offset,
                                GpTimerCurrentCount);

    return Value;
}

VOID
HlpOmap4TimerWrite (
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
    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                         GpTimerCurrentCount,
                         (ULONG)NewCount);

    return;
}

KSTATUS
HlpOmap4TimerArm (
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

    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset, GpTimerMode, 0);
    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                         GpTimerLoadCount,
                         0xFFFFFFFF - (ULONG)TickCount);

    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                         GpTimerCurrentCount,
                         0xFFFFFFFF - (ULONG)TickCount);

    Value = GPTIMER_STARTED;
    if (Mode == TimerModePeriodic) {
        Value |= GPTIMER_AUTORELOAD;
    }

    WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset, GpTimerMode, Value);
    if (Timer->Offset == 0) {
        WRITE_TIMER_REGISTER(Timer->Base,
                             GpTimerInterruptEnable,
                             GPTIMER_OVERFLOW_INTERRUPT);

    } else {
        WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                             GpTimerInterruptEnableAlternate,
                             GPTIMER_OVERFLOW_INTERRUPT);
    }

    return STATUS_SUCCESS;
}

VOID
HlpOmap4TimerDisarm (
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
    // Disable all interrupts. The alternate register interface uses a
    // set/clear style for the interrupt mask bits.
    //

    Timer = (PGP_TIMER_DATA)Context;
    if (Timer->Offset == 0) {
        WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptEnable, 0);

    } else {
        WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                             GpTimerInterruptDisable,
                             0x7);
    }

    //
    // Reset all interrupt-pending bits. This register has a unique offset
    // in the alternate interface.
    //

    if (Timer->Offset == 0) {
        WRITE_TIMER_REGISTER(Timer->Base, GpTimerInterruptStatus, 0x7);

    } else {
        WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                             GpTimerInterruptStatusAlternate,
                             0x7);
    }

    return;
}

VOID
HlpOmap4TimerAcknowledgeInterrupt (
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

    if (Timer->Offset == 0) {
        WRITE_TIMER_REGISTER(Timer->Base,
                             GpTimerInterruptStatus,
                             GPTIMER_OVERFLOW_INTERRUPT);

    } else {
        WRITE_TIMER_REGISTER(Timer->Base + Timer->Offset,
                             GpTimerInterruptStatusAlternate,
                             GPTIMER_OVERFLOW_INTERRUPT);
    }

    return;
}


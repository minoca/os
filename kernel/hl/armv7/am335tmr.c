/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am335tmr.c

Abstract:

    This module implements support for the TI AM335x SoC DM timers.

Author:

    Evan Green 6-Jan-2015

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
#include <minoca/soc/am335x.h>
#include "am335.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an AM335 timer. _Base should be a pointer, and
// _Register should be a AM335_TIMER_REGISTER value.
//

#define READ_TIMER_REGISTER(_Base, _Register) \
    HlReadRegister32((_Base) + (_Register))

//
// This macro writes to an AM335 timer. _Base should be a pointer,
// _Register should be AM335_TIMER_REGISTER value, and _Value should be a
// ULONG.
//

#define WRITE_TIMER_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((_Base) + (_Register), (_Value))

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

    PhysicalAddress - Stores the physical address of the timer.

    Index - Stores the zero-based index of this timer number.

--*/

typedef struct _AM335_TIMER_DATA {
    PVOID Base;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Index;
} AM335_TIMER_DATA, *PAM335_TIMER_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpAm335TimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpAm335TimerRead (
    PVOID Context
    );

VOID
HlpAm335TimerWrite (
    PVOID Context,
    ULONGLONG NewCount
    );

KSTATUS
HlpAm335TimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpAm335TimerDisarm (
    PVOID Context
    );

VOID
HlpAm335TimerAcknowledgeInterrupt (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

PAM335X_TABLE HlAm335Table = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpAm335TimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the AM335 DM Timer hardware module.
    Its role is to detect and report the prescense of AM335 Timers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    TIMER_DESCRIPTION Timer;
    PAM335_TIMER_DATA TimerData;
    ULONG TimerIndex;

    HlAm335Table = HlGetAcpiTable(AM335X_SIGNATURE, NULL);
    if (HlAm335Table == NULL) {
        goto Am335TimerModuleEntryEnd;
    }

    //
    // Fire up the timer's power.
    //

    Status = HlpAm335InitializePowerAndClocks();
    if (!KSUCCESS(Status)) {
        goto Am335TimerModuleEntryEnd;
    }

    //
    // Register each of the independent timers in the timer block.
    //

    for (TimerIndex = 0; TimerIndex < AM335X_TIMER_COUNT; TimerIndex += 1) {

        //
        // Skip the timer if it has no address.
        //

        if (HlAm335Table->TimerBase[TimerIndex] == 0) {
            continue;
        }

        //
        // Skip timer 1 for now, as it has funky register offsets and not that
        // many timers are needed. Skip timer 0 as it seems to interact with
        // power management.
        //

        if ((TimerIndex == 1) || (TimerIndex == 0)) {
            continue;
        }

        RtlZeroMemory(&Timer, sizeof(TIMER_DESCRIPTION));
        Timer.TableVersion = TIMER_DESCRIPTION_VERSION;
        Timer.FunctionTable.Initialize = HlpAm335TimerInitialize;
        Timer.FunctionTable.ReadCounter = HlpAm335TimerRead;
        Timer.FunctionTable.WriteCounter = HlpAm335TimerWrite;
        Timer.FunctionTable.Arm = HlpAm335TimerArm;
        Timer.FunctionTable.Disarm = HlpAm335TimerDisarm;
        Timer.FunctionTable.AcknowledgeInterrupt =
                                             HlpAm335TimerAcknowledgeInterrupt;

        TimerData = HlAllocateMemory(sizeof(AM335_TIMER_DATA),
                                     AM335_ALLOCATION_TAG,
                                     FALSE,
                                     NULL);

        if (TimerData == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Am335TimerModuleEntryEnd;
        }

        RtlZeroMemory(TimerData, sizeof(AM335_TIMER_DATA));
        TimerData->PhysicalAddress = HlAm335Table->TimerBase[TimerIndex];
        TimerData->Index = TimerIndex;
        Timer.Context = TimerData;
        Timer.Features = TIMER_FEATURE_READABLE |
                         TIMER_FEATURE_WRITABLE |
                         TIMER_FEATURE_PERIODIC |
                         TIMER_FEATURE_ONE_SHOT;

        Timer.CounterBitWidth = AM335_TIMER_BIT_WIDTH;

        //
        // The first two timers run at a fixed frequency, but the rest run at
        // the system clock rate.
        //

        if ((TimerIndex == 0) || (TimerIndex == 1)) {
            Timer.CounterFrequency = AM335_TIMER_FREQUENCY_32KHZ;

        } else {
            Timer.CounterFrequency = 0;
        }

        Timer.Interrupt.Line.Type = InterruptLineControllerSpecified;
        Timer.Interrupt.Line.U.Local.Controller = 0;
        Timer.Interrupt.Line.U.Local.Line = HlAm335Table->TimerGsi[TimerIndex];
        Timer.Interrupt.TriggerMode = InterruptModeLevel;
        Timer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;
        Timer.Identifier = TimerIndex;

        //
        // Register the timer with the system.
        //

        Status = HlRegisterHardware(HardwareModuleTimer, &Timer);
        if (!KSUCCESS(Status)) {
            goto Am335TimerModuleEntryEnd;
        }
    }

Am335TimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpAm335TimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an AM335 timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    PAM335_TIMER_DATA Timer;
    ULONG Value;

    Timer = (PAM335_TIMER_DATA)Context;

    //
    // Map the hardware if that has not been done.
    //

    if (Timer->Base == NULL) {
        Timer->Base = HlMapPhysicalAddress(Timer->PhysicalAddress,
                                           AM335_TIMER_CONTROLLER_SIZE,
                                           TRUE);

        if (Timer->Base == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Am335TimerInitializeEnd;
        }
    }

    //
    // Program the timer in free running mode with no interrupt.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerOcpConfig,
                         AM335_TIMER_IDLEMODE_SMART);

    //
    // Disable wakeup functionality.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerInterruptWakeEnable,
                         0);

    //
    // Set the synchronous interface configuration register to non-posted mode,
    // which means that writes don't return until they complete. Posted mode
    // is faster for writes but requires polling a bit for reads.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerSynchronousInterfaceControl,
                         0);

    //
    // Disable all interrupts for now. The alternate register interface uses a
    // set/clear style for the interrupt mask bits.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerInterruptEnableClear,
                         AM335_TIMER_INTERRUPT_MASK);

    //
    // Set the load value to zero to create a free-running timer, and reset the
    // current counter now too.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerLoad, 0);
    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerCount, 0);

    //
    // Set the mode register to auto-reload, and start the timer.
    //

    Value = AM335_TIMER_OVERFLOW_TRIGGER |
            AM335_TIMER_STARTED |
            AM335_TIMER_AUTORELOAD;

    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerControl, Value);

    //
    // Reset all interrupt-pending bits.
    //
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerInterruptStatus,
                         AM335_TIMER_INTERRUPT_MASK);

    Status = STATUS_SUCCESS;

Am335TimerInitializeEnd:
    return Status;
}

ULONGLONG
HlpAm335TimerRead (
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

    PAM335_TIMER_DATA Timer;
    ULONG Value;

    Timer = (PAM335_TIMER_DATA)Context;
    Value = READ_TIMER_REGISTER(Timer->Base, Am335TimerCount);
    return Value;
}

VOID
HlpAm335TimerWrite (
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

    PAM335_TIMER_DATA Timer;

    Timer = (PAM335_TIMER_DATA)Context;
    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerCount, (ULONG)NewCount);
    return;
}

KSTATUS
HlpAm335TimerArm (
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

    PAM335_TIMER_DATA Timer;
    ULONG Value;

    Timer = (PAM335_TIMER_DATA)Context;
    if (TickCount >= MAX_ULONG) {
        TickCount = MAX_ULONG - 1;
    }

    if (TickCount < 2) {
        TickCount = 2;
    }

    //
    // Start the timer ticking.
    //

    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerControl, 0);
    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerLoad, 0 - (ULONG)TickCount);
    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerCount, 0 - (ULONG)TickCount);
    Value = AM335_TIMER_STARTED;
    if (Mode == TimerModePeriodic) {
        Value |= AM335_TIMER_AUTORELOAD;
    }

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerInterruptEnableSet,
                         AM335_TIMER_OVERFLOW_INTERRUPT);

    WRITE_TIMER_REGISTER(Timer->Base, Am335TimerControl, Value);
    return STATUS_SUCCESS;
}

VOID
HlpAm335TimerDisarm (
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

    PAM335_TIMER_DATA Timer;

    //
    // Disable all interrupts.
    //

    Timer = (PAM335_TIMER_DATA)Context;
    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerInterruptEnableClear,
                         AM335_TIMER_INTERRUPT_MASK);

    //
    // Reset all pending interrupt bits.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerInterruptStatus,
                         AM335_TIMER_INTERRUPT_MASK);

    return;
}

VOID
HlpAm335TimerAcknowledgeInterrupt (
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

    PAM335_TIMER_DATA Timer;

    Timer = (PAM335_TIMER_DATA)Context;

    //
    // Clear the overflow interrupt by writing a 1 to the status bit.
    //

    WRITE_TIMER_REGISTER(Timer->Base,
                         Am335TimerInterruptStatus,
                         AM335_TIMER_OVERFLOW_INTERRUPT);

    return;
}


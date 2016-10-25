/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    apictimr.c

Abstract:

    This module implements support for the local APIC timer.

Author:

    Evan Green 28-Oct-2012

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
#include "apic.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpApicTimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpApicTimerRead (
    PVOID Context
    );

VOID
HlpApicTimerWrite (
    PVOID Context,
    ULONGLONG NewCount
    );

KSTATUS
HlpApicTimerArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpApicTimerDisarm (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpApicTimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the APIC hardware module. Its role is to
    detect and report the prescense of a local APIC timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    TIMER_DESCRIPTION ApicTimer;
    KSTATUS Status;

    if (HlApicMadt == NULL) {
        goto ApicTimerModuleEntryEnd;
    }

    RtlZeroMemory(&ApicTimer, sizeof(TIMER_DESCRIPTION));
    ApicTimer.TableVersion = TIMER_DESCRIPTION_VERSION;
    ApicTimer.FunctionTable.Initialize = HlpApicTimerInitialize;
    ApicTimer.FunctionTable.ReadCounter = HlpApicTimerRead;
    ApicTimer.FunctionTable.WriteCounter = HlpApicTimerWrite;
    ApicTimer.FunctionTable.Arm = HlpApicTimerArm;
    ApicTimer.FunctionTable.Disarm = HlpApicTimerDisarm;
    ApicTimer.FunctionTable.AcknowledgeInterrupt = NULL;
    ApicTimer.Context = NULL;
    ApicTimer.Features = TIMER_FEATURE_PER_PROCESSOR |
                         TIMER_FEATURE_READABLE |
                         TIMER_FEATURE_WRITABLE |
                         TIMER_FEATURE_PERIODIC |
                         TIMER_FEATURE_ONE_SHOT;

    //
    // The timer's frequency is not hardcoded, as it runs at the primary bus
    // frequency.
    //

    ApicTimer.CounterFrequency = 0;
    ApicTimer.CounterBitWidth = 32;
    ApicTimer.Interrupt.Line.Type = InterruptLineControllerSpecified;
    ApicTimer.Interrupt.Line.U.Local.Controller = HlFirstIoApicId;
    ApicTimer.Interrupt.Line.U.Local.Line = ApicLineTimer;
    ApicTimer.Interrupt.TriggerMode = InterruptModeEdge;
    ApicTimer.Interrupt.ActiveLevel = InterruptActiveHigh;

    //
    // Register the timer with the system.
    //

    Status = HlRegisterHardware(HardwareModuleTimer, &ApicTimer);
    if (!KSUCCESS(Status)) {
        goto ApicTimerModuleEntryEnd;
    }

ApicTimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpApicTimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes the APIC timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    ULONG LvtValue;

    //
    // Set up an initial value for the APIC timer LVT that masks the timer's
    // interrupt. The APIC will send an "Invalid Vector Received" error
    // interrupt if the LVT is programmed with an invalid LVT, even if the
    // interrupt is masked. Use a valid vector (0x80) even though the interrupt
    // will never fire in this configuration.
    //

    LvtValue = APIC_TIMER_PERIODIC | APIC_LVT_DISABLED | 0x80;
    WRITE_LOCAL_APIC(ApicTimerVector, LvtValue);

    //
    // Set the divisor to divide by 1.
    //

    WRITE_LOCAL_APIC(ApicTimerDivideConfiguration, APIC_TIMER_DIVIDE_BY_1);

    //
    // Start the counter counting.
    //

    WRITE_LOCAL_APIC(ApicTimerInitialCount, 0xFFFFFFFF);
    return STATUS_SUCCESS;
}

ULONGLONG
HlpApicTimerRead (
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

    return 0xFFFFFFFF - READ_LOCAL_APIC(ApicTimerCurrentCount);
}

VOID
HlpApicTimerWrite (
    PVOID Context,
    ULONGLONG NewCount
    )

/*++

Routine Description:

    This routine writes to the timer's hardware counter.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewCount - Supplies the value to write into the counter. It is expected that
        the counter will not stop after the write.

Return Value:

    None.

--*/

{

    WRITE_LOCAL_APIC(ApicTimerCurrentCount, 0xFFFFFFFF - (ULONG)NewCount);
    return;
}

KSTATUS
HlpApicTimerArm (
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

    ULONG ControlValue;
    ULONG ResetValue;

    ResetValue = READ_LOCAL_APIC(ApicTimerVector);
    ResetValue &= ~(APIC_LVT_DISABLED | APIC_TIMER_PERIODIC);
    ControlValue = ResetValue | APIC_LVT_ENABLED;
    ResetValue |= APIC_LVT_DISABLED;
    if (Mode == TimerModePeriodic) {
        ControlValue |= APIC_TIMER_PERIODIC;
    }

    if (TickCount == 0) {
        TickCount = 1;

    } else if (TickCount >= MAX_ULONG) {
        TickCount = MAX_ULONG - 1;
    }

    //
    // Program the timer. As soon as the initial count is written, the value is
    // copied to the current count and the timer begins.
    //

    WRITE_LOCAL_APIC(ApicTimerVector, ResetValue);
    WRITE_LOCAL_APIC(ApicTimerInitialCount, 0);
    WRITE_LOCAL_APIC(ApicTimerVector, ControlValue);
    WRITE_LOCAL_APIC(ApicTimerInitialCount, (ULONG)TickCount);
    return STATUS_SUCCESS;
}

VOID
HlpApicTimerDisarm (
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

    ULONG ControlValue;

    //
    // Disable the APIC timer, turn off periodic mode and set the count to zero.
    //

    ControlValue = READ_LOCAL_APIC(ApicTimerVector);
    ControlValue &= ~APIC_TIMER_PERIODIC;
    ControlValue |= APIC_LVT_DISABLED;
    WRITE_LOCAL_APIC(ApicTimerVector, ControlValue);
    WRITE_LOCAL_APIC(ApicTimerInitialCount, 0x0);
    return;
}


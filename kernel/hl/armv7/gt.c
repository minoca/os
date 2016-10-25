/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gt.c

Abstract:

    This module implements timer support for the ARM Generic Timer.

Author:

    Chris Stevens 23-May-2016

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
#include <minoca/kernel/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the GT allocation tag.
//

#define GT_ALLOCATION_TAG 0x524D5447 //'RMTG'

//
// Define the bits for a generic timer control register.
//

#define GT_CONTROL_INTERRUPT_STATUS_ASSERTED 0x00000004
#define GT_CONTROL_INTERRUPT_MASKED          0x00000002
#define GT_CONTROL_TIMER_ENABLE              0x00000001

//
// --------------------------------------------------------------------- Macros
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpGtInitialize (
    PVOID Context
    );

ULONGLONG
HlpGtRead (
    PVOID Context
    );

KSTATUS
HlpGtArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpGtDisarm (
    PVOID Context
    );

VOID
HlpGtAcknowledgeInterrupt (
    PVOID Context
    );

ULONG
HlpGtGetFrequency (
    VOID
    );

VOID
HlpGtSetVirtualTimerControl (
    ULONG Control
    );

ULONGLONG
HlpGtGetVirtualCount (
    VOID
    );

VOID
HlpGtSetVirtualTimerCompare (
    ULONGLONG CompareValue
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpGtModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the Integrator/CP timer hardware module.
    Its role is to detect and report the prescense of an Integrator/CP timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ARM_CPUID Cpuid;
    ULONG Flags;
    ULONG Frequency;
    TIMER_DESCRIPTION Gt;
    PGTDT GtdtTable;
    KSTATUS Status;

    //
    // Determine if the ARM Generic Timer is supported based on the processor
    // features.
    //

    ArCpuid(&Cpuid);
    if ((Cpuid.ProcessorFeatures[1] & CPUID_PROCESSOR1_GENERIC_TIMER_MASK) ==
        CPUID_PROCESSOR1_GENERIC_TIMER_UNSUPPORTED) {

        goto GtModuleEntryEnd;
    }

    //
    // Attempt to find an GTDT. If one exists, then the GT is present.
    //

    GtdtTable = HlGetAcpiTable(GTDT_SIGNATURE, NULL);
    if (GtdtTable == NULL) {
        goto GtModuleEntryEnd;
    }

    RtlZeroMemory(&Gt, sizeof(TIMER_DESCRIPTION));
    Gt.TableVersion = TIMER_DESCRIPTION_VERSION;
    Gt.FunctionTable.Initialize = HlpGtInitialize;
    Gt.FunctionTable.ReadCounter = HlpGtRead;
    Gt.FunctionTable.Arm = HlpGtArm;
    Gt.FunctionTable.Disarm = HlpGtDisarm;
    Gt.FunctionTable.AcknowledgeInterrupt = HlpGtAcknowledgeInterrupt;

    //
    // Get the frequency from the Generic Timer frequency register. The
    // firmware should have programmed this correctly.
    //

    Frequency = HlpGtGetFrequency();

    //
    // Only use the virtual timer. This could potentially allow for this module
    // to run on top of a hypervisor. Since this timer uses a compare register
    // to trigger interrupts, mark it as absolute and one-shot.
    //

    Gt.Features = TIMER_FEATURE_ABSOLUTE |
                  TIMER_FEATURE_ONE_SHOT |
                  TIMER_FEATURE_READABLE |
                  TIMER_FEATURE_PER_PROCESSOR;

    Gt.CounterBitWidth = 64;
    Gt.CounterFrequency = Frequency;
    Gt.Interrupt.Line.Type = InterruptLineControllerSpecified;
    Gt.Interrupt.Line.U.Local.Controller = 0;
    Gt.Interrupt.Line.U.Local.Line = GtdtTable->VirtualTimerGsi;
    Flags = GtdtTable->VirtualTimerFlags;
    if ((Flags & GTDT_TIMER_FLAG_INTERRUPT_MODE_EDGE) != 0) {
        Gt.Interrupt.TriggerMode = InterruptModeEdge;

    } else {
        Gt.Interrupt.TriggerMode = InterruptModeLevel;
    }

    if ((Flags & GTDT_TIMER_FLAG_INTERRUPT_POLARITY_ACTIVE_LOW) != 0) {
        Gt.Interrupt.ActiveLevel = InterruptActiveLow;

    } else {
        Gt.Interrupt.ActiveLevel = InterruptActiveHigh;
    }

    Status = HlRegisterHardware(HardwareModuleTimer, &Gt);
    if (!KSUCCESS(Status)) {
        goto GtModuleEntryEnd;
    }

GtModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpGtInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an ARM Generic Timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    //
    // The timer is already running, just make sure interrupts are off.
    //

    HlpGtSetVirtualTimerControl(0);
    return STATUS_SUCCESS;
}

ULONGLONG
HlpGtRead (
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

    return HlpGtGetVirtualCount();
}

KSTATUS
HlpGtArm (
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

    if (Mode == TimerModePeriodic) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // The tick count is relative in one shot mode, but the GT can only be
    // armed with an absolute time. Add the current time.
    //

    if (Mode == TimerModeOneShot) {
        TickCount += HlpGtGetVirtualCount();
    }

    HlpGtSetVirtualTimerCompare(TickCount);
    HlpGtSetVirtualTimerControl(GT_CONTROL_TIMER_ENABLE);
    return STATUS_SUCCESS;
}

VOID
HlpGtDisarm (
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

    HlpGtSetVirtualTimerControl(0);
    return;
}

VOID
HlpGtAcknowledgeInterrupt (
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

    //
    // The only way to stop an interrupt from continuing to fire is to either
    // reprogram the compare register or to disable the interrupt. As the timer
    // must await further instruction, disable the interrupt.
    //

    HlpGtSetVirtualTimerControl(0);
    return;
}


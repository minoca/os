/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    am335int.c

Abstract:

    This module implements support for the INTC interrupt controller in the
    TI AM335x SoCs.

Author:

    Evan Green 7-Jan-2015

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
// This macro reads from an AM335 interrupt controller.
//

#define AM335_INTC_READ(_Base, _Register) \
    HlReadRegister32((_Base) + (_Register))

//
// This macro writes to an AM335 interrupt controller.
//

#define AM335_INTC_WRITE(_Base, _Register, _Value) \
    HlWriteRegister32((_Base) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the internal state associated with an AM335 INTC
    interrupt controller.

Members:

    Base - Stores the virtual address of the timer.

    LineCount - Stores the number of lines in the interrupt controller.

    PhysicalAddress - Stores the physical address of the timer.

--*/

typedef struct _AM335_INTC_DATA {
    PVOID Base;
    ULONG LineCount;
    PHYSICAL_ADDRESS PhysicalAddress;
} AM335_INTC_DATA, *PAM335_INTC_DATA;

/*++

Structure Description:

    This structure stores the internal state of the AM335 interrupt controller,
    which can be saved and restored when context is lost.

Members:

    SysConfig - Stores the value of the system configuration register.

    SirIrq - Stores the value of the active interrupt number.

    SirFiq - Stores the value of the active fast interrupt number.

    Protection - Stores the protection register value.

    Idle - Stores the idle register value.

    IrqPriority - Stores the interrupt priority register value.

    FiqPriority - Stores the fast interrupt priority register value.

    Threshold - Stores the threshold register value.

    Mask - Stores the blocks of interrupt masks.

    LineConfiguration - Stores the interrupt line configuration registers (the
        first 8 bits of each, which is all that matters).

--*/

typedef struct _AM335_INTC_STATE {
    ULONG SysConfig;
    ULONG SirIrq;
    ULONG SirFiq;
    ULONG Protection;
    ULONG Idle;
    ULONG IrqPriority;
    ULONG FiqPriority;
    ULONG Threshold;
    ULONG Mask[AM335_MAX_INTERRUPT_LINE_BLOCKS];
    UCHAR LineConfiguration[AM335_MAX_INTERRUPT_LINES];
} AM335_INTC_STATE, *PAM335_INTC_STATE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpAm335InterruptInitializeIoUnit (
    PVOID Context
    );

INTERRUPT_CAUSE
HlpAm335InterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
HlpAm335InterruptEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

KSTATUS
HlpAm335InterruptRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

KSTATUS
HlpAm335InterruptSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

VOID
HlpAm335InterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

KSTATUS
HlpAm335InterruptSaveState (
    PVOID Context,
    PVOID Buffer
    );

KSTATUS
HlpAm335InterruptRestoreState (
    PVOID Context,
    PVOID Buffer
    );

KSTATUS
HlpAm335InterruptDescribeLines (
    PAM335_INTC_DATA Data
    );

//
// -------------------------------------------------------------------- Globals
//

INTERRUPT_FUNCTION_TABLE HlAm335InterruptFunctionTable = {
    HlpAm335InterruptInitializeIoUnit,
    HlpAm335InterruptSetLineState,
    HlpAm335InterruptMaskLine,
    HlpAm335InterruptBegin,
    NULL,
    HlpAm335InterruptEndOfInterrupt,
    HlpAm335InterruptRequestInterrupt,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    HlpAm335InterruptSaveState,
    HlpAm335InterruptRestoreState
};

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpAm335InterruptModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the AM335 Interrupt hardware
    module. Its role is to detect and report the prescense of an INTC
    interrupt controller.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PAM335_INTC_DATA Context;
    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    KSTATUS Status;

    HlAm335Table = HlGetAcpiTable(AM335X_SIGNATURE, NULL);

    //
    // Interrupt controllers are always initialized before timers, so the
    // integrator table and services should already be set up.
    //

    if (HlAm335Table == NULL) {
        goto Am335InterruptModuleEntryEnd;
    }

    RtlZeroMemory(&NewController, sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));
    if (HlAm335Table->InterruptControllerBase != 0) {

        //
        // Initialize the new controller structure.
        //

        NewController.TableVersion = INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;
        RtlCopyMemory(&(NewController.FunctionTable),
                      &HlAm335InterruptFunctionTable,
                      sizeof(INTERRUPT_FUNCTION_TABLE));

        Context = HlAllocateMemory(sizeof(AM335_INTC_DATA),
                                   AM335_ALLOCATION_TAG,
                                   FALSE,
                                   NULL);

        if (Context == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Am335InterruptModuleEntryEnd;
        }

        RtlZeroMemory(Context, sizeof(AM335_INTC_DATA));
        Context->PhysicalAddress = HlAm335Table->InterruptControllerBase;
        Context->LineCount = HlAm335Table->InterruptLineCount;
        NewController.Context = Context;
        NewController.Identifier = 0;
        NewController.ProcessorCount = 0;
        NewController.PriorityCount = AM335_INTC_PRIORITY_COUNT;
        NewController.SaveContextSize = sizeof(AM335_INTC_STATE);

        //
        // Register the controller with the system.
        //

        Status = HlRegisterHardware(HardwareModuleInterruptController,
                                    &NewController);

        if (!KSUCCESS(Status)) {
            goto Am335InterruptModuleEntryEnd;
        }
    }

Am335InterruptModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpAm335InterruptInitializeIoUnit (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an interrupt controller. It's responsible for
    masking all interrupt lines on the controller and setting the current
    priority to the lowest (allow all interrupts). Once completed successfully,
    it is expected that interrupts can be enabled at the processor core with
    no interrupts occurring.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PAM335_INTC_DATA Data;
    KSTATUS Status;
    ULONG Value;

    Data = Context;
    if (Data->Base == NULL) {
        Data->Base = HlMapPhysicalAddress(Data->PhysicalAddress,
                                          AM335_INTC_CONTROLLER_SIZE,
                                          TRUE);

        if (Data->Base == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Am335InterruptInitializeIoUnitEnd;
        }

        Status = HlpAm335InterruptDescribeLines(Data);
        if (!KSUCCESS(Status)) {
            goto Am335InterruptInitializeIoUnitEnd;
        }
    }

    //
    // Reset the interrupt controller. This masks all lines.
    //

    Value = AM335_INTC_SYSTEM_CONFIG_SOFT_RESET;
    AM335_INTC_WRITE(Data->Base, Am335IntcSystemConfig, Value);
    do {
        Value = AM335_INTC_READ(Data->Base, Am335IntcSystemStatus);

    } while ((Value & AM335_INTC_SYSTEM_STATUS_RESET_DONE) == 0);

    AM335_INTC_WRITE(Data->Base,
                     Am335IntcSystemConfig,
                     AM335_INTC_SYSTEM_CONFIG_AUTO_IDLE);

    //
    // Make sure only privileged mode can access the registers.
    //

    AM335_INTC_WRITE(Data->Base,
                     Am335IntcProtection,
                     AM335_INTC_PROTECTION_ENABLE);

    //
    // Allow the input synchronizer clock to auto-idle based on input activity.
    //

    AM335_INTC_WRITE(Data->Base,
                     Am335IntcIdle,
                     AM335_INTC_IDLE_INPUT_AUTO_GATING);

    //
    // Set the current priority to be the lowest, so all interrupts come in
    // (if they were to be unmasked).
    //

    AM335_INTC_WRITE(Data->Base, Am335IntcThreshold, AM335_INTC_PRIORITY_COUNT);
    Status = STATUS_SUCCESS;

Am335InterruptInitializeIoUnitEnd:
    return Status;
}

INTERRUPT_CAUSE
HlpAm335InterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    )

/*++

Routine Description:

    This routine is called when an interrupt fires. Its role is to determine
    if an interrupt has fired on the given controller, accept it, and determine
    which line fired if any. This routine will always be called with interrupts
    disabled at the processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    FiringLine - Supplies a pointer where the interrupt hardware module will
        fill in which line fired, if applicable.

    MagicCandy - Supplies a pointer where the interrupt hardware module can
        store 32 bits of private information regarding this interrupt. This
        information will be returned to it when the End Of Interrupt routine
        is called.

Return Value:

    Returns an interrupt cause indicating whether or not an interrupt line,
    spurious interrupt, or no interrupt fired on this controller.

--*/

{

    ULONG ActiveIrq;
    ULONG ActiveIrqPriority;
    PAM335_INTC_DATA Data;

    Data = Context;

    //
    // Get the currently asserting line. If it's a spurious interrupt, return
    // immediately.
    //

    ActiveIrq = AM335_INTC_READ(Data->Base, Am335IntcSortedIrq);
    if ((ActiveIrq & AM335_INTC_SORTED_SPURIOUS) != 0) {
        return InterruptCauseSpuriousInterrupt;
    }

    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->U.Local.Controller = 0;
    FiringLine->U.Local.Line = ActiveIrq;

    //
    // Save the old priority into the magic candy, and then set the priority
    // to the priority of the interrupting source.
    //

    ActiveIrqPriority = AM335_INTC_READ(Data->Base, Am335IntcIrqPriority);
    *MagicCandy = AM335_INTC_READ(Data->Base, Am335IntcThreshold);
    AM335_INTC_WRITE(Data->Base, Am335IntcThreshold, ActiveIrqPriority);

    //
    // Write the New IRQ Agreement bit so that additional interrupts of higher
    // priority can come in.
    //

    AM335_INTC_WRITE(Data->Base,
                     Am335IntcControl,
                     AM335_INTC_CONTROL_NEW_IRQ_AGREEMENT);

    return InterruptCauseLineFired;
}

VOID
HlpAm335InterruptEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    )

/*++

Routine Description:

    This routine is called after an interrupt has fired and been serviced. Its
    role is to tell the interrupt controller that processing has completed.
    This routine will always be called with interrupts disabled at the
    processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    MagicCandy - Supplies the magic candy that that the interrupt hardware
        module stored when the interrupt began.

Return Value:

    None.

--*/

{

    PAM335_INTC_DATA Data;

    Data = Context;

    //
    // The magic candy value contained the priority register when this interrupt
    // began. Restore that value.
    //

    AM335_INTC_WRITE(Data->Base, Am335IntcThreshold, MagicCandy);
    return;
}

KSTATUS
HlpAm335InterruptRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    )

/*++

Routine Description:

    This routine requests a hardware interrupt on the given line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the interrupt line to spark.

    Vector - Supplies the vector to generate the interrupt on (for vectored
        architectures only).

    Target - Supplies a pointer to the set of processors to target.

Return Value:

    STATUS_SUCCESS on success.

    Error code on failure.

--*/

{

    //
    // This feature will be implemented when it is required (probably by
    // power management).
    //

    return STATUS_NOT_IMPLEMENTED;
}

KSTATUS
HlpAm335InterruptSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    )

/*++

Routine Description:

    This routine enables or disables and configures an interrupt line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to set up. This will always be a
        controller specified line.

    State - Supplies a pointer to the new configuration of the line.

    ResourceData - Supplies an optional pointer to the device specific resource
        data for the interrupt line.

    ResourceDataSize - Supplies the size of the resource data, in bytes.

Return Value:

    Status code.

--*/

{

    PAM335_INTC_DATA Data;
    ULONG Index;
    ULONG LocalLine;
    KSTATUS Status;
    ULONG Value;

    Data = Context;
    LocalLine = Line->U.Local.Line;
    if ((Line->Type != InterruptLineControllerSpecified) ||
        (Line->U.Local.Controller != 0) ||
        (LocalLine >= Data->LineCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Am335InterruptSetLineStateEnd;
    }

    if ((State->Output.Type != InterruptLineControllerSpecified) ||
        (State->Output.U.Local.Controller != INTERRUPT_CPU_IDENTIFIER) ||
        (State->Output.U.Local.Line != INTERRUPT_CPU_IRQ_PIN)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Am335InterruptSetLineStateEnd;
    }

    //
    // Set the priority of the new interrupt.
    //

    Value = (AM335_INTC_PRIORITY_COUNT - State->HardwarePriority) + 1;
    Value = Value << AM335_INTC_LINE_PRIORITY_SHIFT;
    AM335_INTC_WRITE(Data->Base, AM335_INTC_LINE(LocalLine), Value);

    //
    // To enable, clear the interrupt mask.
    //

    Index = AM335_INTC_LINE_TO_INDEX(LocalLine);
    Value = AM335_INTC_LINE_TO_MASK(LocalLine);
    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) != 0) {
        AM335_INTC_WRITE(Data->Base, AM335_INTC_MASK_CLEAR(Index), Value);

    //
    // To disable, set the interrupt mask.
    //

    } else {
        AM335_INTC_WRITE(Data->Base, AM335_INTC_MASK_SET(Index), Value);
    }

    Status = STATUS_SUCCESS;

Am335InterruptSetLineStateEnd:
    return Status;
}

VOID
HlpAm335InterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    )

/*++

Routine Description:

    This routine masks or unmasks an interrupt line, leaving the rest of the
    line state intact.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to maek or unmask. This will always
        be a controller specified line.

    Enable - Supplies a boolean indicating whether to mask the interrupt,
        preventing interrupts from coming through (FALSE), or enable the line
        and allow interrupts to come through (TRUE).

Return Value:

    None.

--*/

{

    PAM335_INTC_DATA Data;
    ULONG Index;
    ULONG Value;

    Data = Context;
    Index = AM335_INTC_LINE_TO_INDEX(Line->U.Local.Line);
    Value = AM335_INTC_LINE_TO_MASK(Line->U.Local.Line);
    if (Enable != FALSE) {
        AM335_INTC_WRITE(Data->Base, AM335_INTC_MASK_CLEAR(Index), Value);

    } else {
        AM335_INTC_WRITE(Data->Base, AM335_INTC_MASK_SET(Index), Value);
    }

    return;
}

KSTATUS
HlpAm335InterruptSaveState (
    PVOID Context,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine saves the current state of the interrupt controller, which
    may lost momentarily in the hardware due to a power transition.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Buffer - Supplies a pointer to the save buffer for this processor, the
        size of which was reported during registration.

Return Value:

    Status code.

--*/

{

    PAM335_INTC_DATA Data;
    ULONG Index;
    PAM335_INTC_STATE State;

    Data = Context;
    State = Buffer;
    State->SysConfig = AM335_INTC_READ(Data->Base, Am335IntcSystemConfig);
    State->SirIrq = AM335_INTC_READ(Data->Base, Am335IntcSortedIrq);
    State->SirFiq = AM335_INTC_READ(Data->Base, Am335IntcSortedFiq);
    State->Protection = AM335_INTC_READ(Data->Base, Am335IntcProtection);
    State->Idle = AM335_INTC_READ(Data->Base, Am335IntcIdle);
    State->IrqPriority = AM335_INTC_READ(Data->Base, Am335IntcIrqPriority);
    State->FiqPriority = AM335_INTC_READ(Data->Base, Am335IntcFiqPriority);
    State->Threshold = AM335_INTC_READ(Data->Base, Am335IntcThreshold);
    for (Index = 0; Index < AM335_MAX_INTERRUPT_LINE_BLOCKS; Index += 1) {
        State->Mask[Index] = AM335_INTC_READ(Data->Base,
                                             Am335IntcMask + (Index * 0x20));
    }

    for (Index = 0; Index < AM335_MAX_INTERRUPT_LINES; Index += 1) {
        State->LineConfiguration[Index] =
                           AM335_INTC_READ(Data->Base, AM335_INTC_LINE(Index));
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpAm335InterruptRestoreState (
    PVOID Context,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine restores the previous state of the interrupt controller.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Buffer - Supplies a pointer to the save buffer for this processor, the
        size of which was reported during registration.

Return Value:

    Status code.

--*/

{

    PAM335_INTC_DATA Data;
    ULONG Index;
    PAM335_INTC_STATE State;
    ULONG Value;

    Data = Context;
    State = Buffer;

    //
    // Reset the thing first, and set some sane defaults.
    //

    HlpAm335InterruptInitializeIoUnit(Context);
    AM335_INTC_WRITE(Data->Base, Am335IntcSystemConfig, State->SysConfig);
    AM335_INTC_WRITE(Data->Base, Am335IntcSortedIrq, State->SirIrq);
    AM335_INTC_WRITE(Data->Base, Am335IntcSortedFiq, State->SirFiq);
    AM335_INTC_WRITE(Data->Base, Am335IntcProtection, State->Protection);
    AM335_INTC_WRITE(Data->Base, Am335IntcIdle, State->Idle);
    AM335_INTC_WRITE(Data->Base, Am335IntcIrqPriority, State->IrqPriority);
    AM335_INTC_WRITE(Data->Base, Am335IntcFiqPriority, State->FiqPriority);
    AM335_INTC_WRITE(Data->Base, Am335IntcThreshold, State->Threshold);

    //
    // Restore the line configurations before unmasking anything.
    //

    for (Index = 0; Index < AM335_MAX_INTERRUPT_LINES; Index += 1) {
        Value = State->LineConfiguration[Index];
        if (Value == 0) {
            continue;
        }

        AM335_INTC_WRITE(Data->Base, AM335_INTC_LINE(Index), Value);
    }

    //
    // Write the masks, which start out all ones. Clear (enable) anything
    // that's not set in the structure values.
    //

    for (Index = 0; Index < AM335_MAX_INTERRUPT_LINE_BLOCKS; Index += 1) {
        AM335_INTC_WRITE(Data->Base,
                         Am335IntcMaskClear + (0x20 * Index),
                         ~State->Mask[Index]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpAm335InterruptDescribeLines (
    PAM335_INTC_DATA Data
    )

/*++

Routine Description:

    This routine describes all interrupt lines to the system.

Arguments:

    Data - Supplies a pointer to the interrupt controller context.

Return Value:

    Status code.

--*/

{

    INTERRUPT_LINES_DESCRIPTION Lines;
    KSTATUS Status;

    RtlZeroMemory(&Lines, sizeof(INTERRUPT_LINES_DESCRIPTION));
    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;

    //
    // Describe the normal lines on the INTC.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.Controller = 0;
    Lines.LineStart = 0;
    Lines.LineEnd = Data->LineCount;
    Lines.Gsi = 0;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto Am335InterruptDescribeLinesEnd;
    }

    //
    // Register the output lines.
    //

    Lines.Type = InterruptLinesOutput;
    Lines.OutputControllerIdentifier = INTERRUPT_CPU_IDENTIFIER;
    Lines.LineStart = INTERRUPT_ARM_MIN_CPU_LINE;
    Lines.LineEnd = INTERRUPT_ARM_MAX_CPU_LINE;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto Am335InterruptDescribeLinesEnd;
    }

Am335InterruptDescribeLinesEnd:
    return Status;
}


/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
#include "am335x.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from an AM335 interrupt controller.
//

#define AM335_INTC_READ(_Base, _Register) \
    HlAm335KernelServices->ReadRegister32((_Base) + (_Register))

//
// This macro writes to an AM335 interrupt controller.
//

#define AM335_INTC_WRITE(_Base, _Register, _Value)                     \
    HlAm335KernelServices->WriteRegister32((_Base) + (_Register), (_Value))

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
    PINTERRUPT_LINE_STATE State
    );

KSTATUS
HlpAm335InterruptDescribeLines (
    PAM335_INTC_DATA Data
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpAm335InterruptModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    )

/*++

Routine Description:

    This routine is the entry point for the AM335 Interrupt hardware
    module. Its role is to detect and report the prescense of an INTC
    interrupt controller.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    PAM335_INTC_DATA Context;
    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    KSTATUS Status;

    HlAm335KernelServices = Services;
    HlAm335Table = HlAm335KernelServices->GetAcpiTable(AM335X_SIGNATURE, NULL);

    //
    // Interrupt controllers are always initialized before timers, so the
    // integrator table and services should already be set up.
    //

    if (HlAm335Table == NULL) {
        goto Am335InterruptModuleEntryEnd;
    }

    Services->ZeroMemory(&NewController,
                         sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));

    if (HlAm335Table->InterruptControllerBase != INVALID_PHYSICAL_ADDRESS) {

        //
        // Initialize the new controller structure.
        //

        NewController.TableVersion = INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;
        NewController.FunctionTable.EnumerateProcessors = NULL;
        NewController.FunctionTable.InitializeLocalUnit = NULL;
        NewController.FunctionTable.InitializeIoUnit =
                                             HlpAm335InterruptInitializeIoUnit;

        NewController.FunctionTable.SetLocalUnitAddressing = NULL;
        NewController.FunctionTable.FastEndOfInterrupt = NULL;
        NewController.FunctionTable.BeginInterrupt = HlpAm335InterruptBegin;
        NewController.FunctionTable.EndOfInterrupt =
                                               HlpAm335InterruptEndOfInterrupt;

        NewController.FunctionTable.RequestInterrupt =
                                             HlpAm335InterruptRequestInterrupt;

        NewController.FunctionTable.StartProcessor = NULL;
        NewController.FunctionTable.SetLineState =
                                                 HlpAm335InterruptSetLineState;

        Context = HlAm335KernelServices->AllocateMemory(
                                                    sizeof(AM335_INTC_DATA),
                                                    AM335_ALLOCATION_TAG,
                                                    FALSE,
                                                    NULL);

        if (Context == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Am335InterruptModuleEntryEnd;
        }

        HlAm335KernelServices->ZeroMemory(Context, sizeof(AM335_INTC_DATA));
        Context->PhysicalAddress = HlAm335Table->InterruptControllerBase;
        Context->LineCount = HlAm335Table->InterruptLineCount;
        NewController.Context = Context;
        NewController.Identifier = 0;
        NewController.ProcessorCount = 0;
        NewController.PriorityCount = AM335_INTC_PRIORITY_COUNT;

        //
        // Register the controller with the system.
        //

        Status = Services->Register(HardwareModuleInterruptController,
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
        Data->Base = HlAm335KernelServices->MapPhysicalAddress(
                                                    Data->PhysicalAddress,
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
    FiringLine->Controller = 0;
    FiringLine->Line = ActiveIrq;

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
    PINTERRUPT_LINE_STATE State
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

Return Value:

    Status code.

--*/

{

    PAM335_INTC_DATA Data;
    ULONG Index;
    KSTATUS Status;
    ULONG Value;

    Data = Context;
    if ((Line->Type != InterruptLineControllerSpecified) ||
        (Line->Controller != 0) ||
        (Line->Line >= Data->LineCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Am335InterruptSetLineStateEnd;
    }

    if ((State->Output.Type != InterruptLineControllerSpecified) ||
        (State->Output.Controller != INTERRUPT_CPU_IDENTIFIER) ||
        (State->Output.Line != INTERRUPT_CPU_IRQ_PIN)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Am335InterruptSetLineStateEnd;
    }

    //
    // Set the priority of the new interrupt.
    //

    Value = (AM335_INTC_PRIORITY_COUNT - State->HardwarePriority) + 1;
    Value = Value << AM335_INTC_LINE_PRIORITY_SHIFT;
    AM335_INTC_WRITE(Data->Base, AM335_INTC_LINE(Line->Line), Value);

    //
    // To enable, clear the interrupt mask.
    //

    Index = AM335_INTC_LINE_TO_INDEX(Line->Line);
    Value = AM335_INTC_LINE_TO_MASK(Line->Line);
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

    HlAm335KernelServices->ZeroMemory(&Lines,
                                      sizeof(INTERRUPT_LINES_DESCRIPTION));

    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;

    //
    // Describe the normal lines on the INTC.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.Controller = 0;
    Lines.LineStart = 0;
    Lines.LineEnd = Data->LineCount;
    Lines.Gsi = 0;
    Status = HlAm335KernelServices->Register(HardwareModuleInterruptLines,
                                             &Lines);

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
    Status = HlAm335KernelServices->Register(HardwareModuleInterruptLines,
                                             &Lines);

    if (!KSUCCESS(Status)) {
        goto Am335InterruptDescribeLinesEnd;
    }

Am335InterruptDescribeLinesEnd:
    return Status;
}


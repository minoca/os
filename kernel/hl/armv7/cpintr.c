/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cpintr.c

Abstract:

    This module implements interrupt controller support for the Integrator/CP
    board.

Author:

    Evan Green 22-Aug-2012

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
#include "integcp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of soft priority levels implemented in the interrupt
// controller.
//

#define INTEGRATORCP_INTERRUPT_PRIORITY_COUNT 16

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from the Integrator/CP interrupt controller. The parameter
// should be CP_INTERRUPT_REGISTER value.
//

#define READ_INTERRUPT_REGISTER(_Register) \
    HlReadRegister32((PULONG)HlCpInterruptController + (_Register))

//
// This macro writes to the Integrator/CP interrupt controller. _Register
// should be CP_INTERRUPT_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_INTERRUPT_REGISTER(_Register, _Value) \
    HlWriteRegister32((PULONG)HlCpInterruptController + (_Register), (_Value))

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpCpInterruptInitializeIoUnit (
    PVOID Context
    );

INTERRUPT_CAUSE
HlpCpInterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
HlpCpInterruptEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

KSTATUS
HlpCpInterruptRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

KSTATUS
HlpCpInterruptSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

VOID
HlpCpInterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

KSTATUS
HlpCpInterruptDescribeLines (
    VOID
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the offsets to interrupt controller registers, in ULONGs.
//

typedef enum _CP_INTERRUPT_REGISTER {
    CpInterruptIrqStatus              = 0x0,
    CpInterruptIrqRawStatus           = 0x1,
    CpInterruptIrqEnable              = 0x2,
    CpInterruptIrqDisable             = 0x3,
    CpInterruptSoftwareInterruptSet   = 0x4,
    CpInterruptSoftwareInterruptClear = 0x5,
    CpInterruptFiqStatus              = 0x8,
    CpInterruptFiqRawStatus           = 0x9,
    CpInterruptFiqEnable              = 0xA,
    CpInterruptFiqDisable             = 0xB,
} CP_INTERRUPT_REGISTER, *PCP_INTERRUPT_REGISTER;

/*++

Structure Description:

    This structure describes the Integrator/CP private interrupt controller
    state.

Members:

    PhysicalAddress - Stores the physical address of this controller.

    LinePriority - Stores the priority for each interrupt line.

    CurrentPriority - Stores the current priority of the interrupt controller.

    Masks - Stores the mask of interrupts to disable when an interrupt of each
        priority fires.

    EnabledMask - Stores the mask of interrupts enabled at any priority.

--*/

typedef struct _INTEGRATORCP_INTERRUPT_DATA {
    PHYSICAL_ADDRESS PhysicalAddress;
    UCHAR LinePriority[INTEGRATORCP_INTERRUPT_LINE_COUNT];
    UCHAR CurrentPriority;
    ULONG Masks[INTEGRATORCP_INTERRUPT_PRIORITY_COUNT];
    ULONG EnabledMask;
} INTEGRATORCP_INTERRUPT_DATA, *PINTEGRATORCP_INTERRUPT_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// Store the virtual address of the mapped interrupt controller.
//

PVOID HlCpInterruptController = NULL;

//
// Store a pointer to the Integrator/CP ACPI table, if found.
//

PINTEGRATORCP_TABLE HlCpIntegratorTable = NULL;

//
// Define the interrupt function table template.
//

INTERRUPT_FUNCTION_TABLE HlCpInterruptFunctionTable = {
    HlpCpInterruptInitializeIoUnit,
    HlpCpInterruptSetLineState,
    HlpCpInterruptMaskLine,
    HlpCpInterruptBegin,
    NULL,
    HlpCpInterruptEndOfInterrupt,
    HlpCpInterruptRequestInterrupt,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpCpInterruptModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the Integrator/CP Interrupt hardware
    module. Its role is to detect and report the prescense of an Integrator/CP
    interrupt controller.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PINTEGRATORCP_TABLE IntegratorTable;
    PINTEGRATORCP_INTERRUPT_DATA InterruptData;
    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    KSTATUS Status;

    //
    // Attempt to find the Integrator/CP ACPI table.
    //

    IntegratorTable = HlGetAcpiTable(INTEGRATORCP_SIGNATURE, NULL);
    if (IntegratorTable == NULL) {
        goto IntegratorCpInterruptModuleEntryEnd;
    }

    HlCpIntegratorTable = IntegratorTable;

    //
    // Zero out the controller description.
    //

    RtlZeroMemory(&NewController, sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));

    //
    // Allocate context needed for this Interrupt Controller.
    //

    InterruptData = HlAllocateMemory(sizeof(INTEGRATORCP_INTERRUPT_DATA),
                                     INTEGRATOR_ALLOCATION_TAG,
                                     FALSE,
                                     NULL);

    if (InterruptData == NULL) {
        goto IntegratorCpInterruptModuleEntryEnd;
    }

    RtlZeroMemory(InterruptData, sizeof(INTEGRATORCP_INTERRUPT_DATA));
    InterruptData->PhysicalAddress =
                           IntegratorTable->InterruptControllerPhysicalAddress;

    //
    // Initialize the new controller structure.
    //

    NewController.TableVersion = INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;
    RtlCopyMemory(&(NewController.FunctionTable),
                  &HlCpInterruptFunctionTable,
                  sizeof(INTERRUPT_FUNCTION_TABLE));

    NewController.Context = InterruptData;
    NewController.Identifier = 0;
    NewController.ProcessorCount = 0;
    NewController.PriorityCount = INTEGRATORCP_INTERRUPT_PRIORITY_COUNT;

    //
    // Register the controller with the system.
    //

    Status = HlRegisterHardware(HardwareModuleInterruptController,
                                &NewController);

    if (!KSUCCESS(Status)) {
        goto IntegratorCpInterruptModuleEntryEnd;
    }

IntegratorCpInterruptModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpCpInterruptInitializeIoUnit (
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

    PINTEGRATORCP_INTERRUPT_DATA InterruptData;
    KSTATUS Status;

    InterruptData = (PINTEGRATORCP_INTERRUPT_DATA)Context;
    if (HlCpInterruptController == NULL) {
        HlCpInterruptController = HlMapPhysicalAddress(
                                        InterruptData->PhysicalAddress,
                                        INTEGRATORCP_INTERRUPT_CONTROLLER_SIZE,
                                        TRUE);

        if (HlCpInterruptController == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CpInterruptInitializeIoUnitEnd;
        }

        //
        // Describe the interrupt lines on this controller.
        //

        Status = HlpCpInterruptDescribeLines();
        if (!KSUCCESS(Status)) {
            goto CpInterruptInitializeIoUnitEnd;
        }
    }

    //
    // Disable all FIQ and IRQ lines.
    //

    WRITE_INTERRUPT_REGISTER(CpInterruptIrqDisable, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(CpInterruptFiqDisable, 0xFFFFFFFF);
    InterruptData->CurrentPriority = 0;
    InterruptData->EnabledMask = 0;
    Status = STATUS_SUCCESS;

CpInterruptInitializeIoUnitEnd:
    return Status;
}

INTERRUPT_CAUSE
HlpCpInterruptBegin (
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

    ULONG Index;
    PINTEGRATORCP_INTERRUPT_DATA InterruptData;
    ULONG Mask;
    UCHAR Priority;
    ULONG Status;

    InterruptData = (PINTEGRATORCP_INTERRUPT_DATA)Context;
    Status = READ_INTERRUPT_REGISTER(CpInterruptIrqStatus);
    if (Status == 0) {
        return InterruptCauseNoInterruptHere;
    }

    //
    // Find the first firing index.
    //

    Index = 0;
    while ((Status & 0x1) == 0) {
        Status = Status >> 1;
        Index += 1;
    }

    //
    // Disable all interrupts at or below this run level.
    //

    Priority = InterruptData->LinePriority[Index];
    Mask = InterruptData->Masks[Priority];
    WRITE_INTERRUPT_REGISTER(CpInterruptIrqDisable, Mask);

    //
    // Save the previous priority to know what to restore to when this
    // interrupt ends.
    //

    *MagicCandy = InterruptData->CurrentPriority;
    InterruptData->CurrentPriority = Priority;

    //
    // Return the interrupting line's information.
    //

    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->U.Local.Controller = 0;
    FiringLine->U.Local.Line = Index;
    return InterruptCauseLineFired;
}

VOID
HlpCpInterruptEndOfInterrupt (
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

    PINTEGRATORCP_INTERRUPT_DATA InterruptData;
    ULONG Mask;

    //
    // Re-enable interrupts at the previous priority level before this
    // interrupt fired. The enabled mask prevents enabling interrupts that
    // weren't enabled before.
    //

    InterruptData = (PINTEGRATORCP_INTERRUPT_DATA)Context;
    InterruptData->CurrentPriority = MagicCandy;
    Mask = ~InterruptData->Masks[MagicCandy] & InterruptData->EnabledMask;
    WRITE_INTERRUPT_REGISTER(CpInterruptIrqEnable, Mask);
    return;
}

KSTATUS
HlpCpInterruptRequestInterrupt (
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
HlpCpInterruptSetLineState (
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

    ULONG BitMask;
    ULONG Index;
    PINTEGRATORCP_INTERRUPT_DATA InterruptData;
    ULONG LocalLine;
    UCHAR Priority;
    KSTATUS Status;

    InterruptData = (PINTEGRATORCP_INTERRUPT_DATA)Context;
    LocalLine = Line->U.Local.Line;
    if ((Line->Type != InterruptLineControllerSpecified) ||
        (Line->U.Local.Controller != 0) ||
        (LocalLine >= INTEGRATORCP_INTERRUPT_LINE_COUNT)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CpInterruptSetLineStateEnd;
    }

    if ((State->Output.Type != InterruptLineControllerSpecified) ||
        (State->Output.U.Local.Controller != INTERRUPT_CPU_IDENTIFIER) ||
        (State->Output.U.Local.Line != INTERRUPT_CPU_IRQ_PIN)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CpInterruptSetLineStateEnd;
    }

    //
    // Calculate the bit to flip and flip it.
    //

    BitMask = 1 << LocalLine;
    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) != 0) {
        Priority = State->HardwarePriority;
        InterruptData->LinePriority[LocalLine] = Priority;
        InterruptData->EnabledMask |= BitMask;

        //
        // This interrupt gets masked at and above its priority level.
        //

        for (Index = Priority;
             Index < INTEGRATORCP_INTERRUPT_PRIORITY_COUNT;
             Index += 1) {

            InterruptData->Masks[Index] |= BitMask;
        }

        WRITE_INTERRUPT_REGISTER(CpInterruptIrqEnable, BitMask);

    } else {
        WRITE_INTERRUPT_REGISTER(CpInterruptIrqDisable, BitMask);
        InterruptData->EnabledMask &= ~BitMask;

        //
        // Remove this interrupt from the masks.
        //

        for (Index = 0;
             Index < INTEGRATORCP_INTERRUPT_PRIORITY_COUNT;
             Index += 1) {

            InterruptData->Masks[Index] &= ~BitMask;
        }
    }

    Status = STATUS_SUCCESS;

CpInterruptSetLineStateEnd:
    return Status;
}

VOID
HlpCpInterruptMaskLine (
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

    ULONG BitMask;

    //
    // Calculate the bit to flip and flip it.
    //

    BitMask = 1 << Line->U.Local.Line;
    if (Enable != FALSE) {
        WRITE_INTERRUPT_REGISTER(CpInterruptIrqEnable, BitMask);

    } else {
        WRITE_INTERRUPT_REGISTER(CpInterruptIrqDisable, BitMask);
    }

    return;
}

KSTATUS
HlpCpInterruptDescribeLines (
    VOID
    )

/*++

Routine Description:

    This routine describes all interrupt lines to the system.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    INTERRUPT_LINES_DESCRIPTION Lines;
    KSTATUS Status;

    RtlZeroMemory(&Lines, sizeof(INTERRUPT_LINES_DESCRIPTION));
    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;

    //
    // Describe the normal lines on the Integrator/CP.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.Controller = 0;
    Lines.LineStart = 0;
    Lines.LineEnd = INTEGRATORCP_INTERRUPT_LINE_COUNT;
    Lines.Gsi = HlCpIntegratorTable->InterruptControllerGsiBase;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto CpInterruptDescribeLinesEnd;
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
        goto CpInterruptDescribeLinesEnd;
    }

CpInterruptDescribeLinesEnd:
    return Status;
}


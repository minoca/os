/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omapintr.c

Abstract:

    This module implements MPU interrupt controller support for the TI OMAP3
    SoCs.

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
// ---------------------------------------------------------------- Definitions
//

//
// This bit is set if the interrupt routes to the FIQ interrupt.
//

#define MPU_INTERRUPT_ROUTE_TO_FIQ 0x00000001

//
// Define the shift amount for the priority component of an interrupt line
// configuration.
//

#define MPU_INTERRUPT_PRIORITY_SHIFT 2

//
// If any of these bits are set, then the interrupt is spurious.
//

#define MPU_SPURIOUS_INTERRUPT_MASK 0xFFFFFF80

//
// Set this bit to allow new IRQ interrupts to come in.
//

#define MPU_CONTROL_NEW_IRQ_AGREEMENT 0x00000001

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from the OMAP3 interrupt controller. The parameter
// should be MPU_REGISTER value.
//

#define READ_INTERRUPT_REGISTER(_Register)    \
    HlReadRegister32((PULONG)HlOmap3InterruptController + (_Register))

//
// This macro writes to the OMAP3 interrupt controller. _Register
// should be MPU_REGISTER value, and _Value should be a ULONG.
//

#define WRITE_INTERRUPT_REGISTER(_Register, _Value)                     \
    HlWriteRegister32((PULONG)HlOmap3InterruptController + (_Register), \
                      (_Value))

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpOmap3InterruptInitializeIoUnit (
    PVOID Context
    );

INTERRUPT_CAUSE
HlpOmap3InterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
HlpOmap3InterruptEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

KSTATUS
HlpOmap3InterruptRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

KSTATUS
HlpOmap3InterruptSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

VOID
HlpOmap3InterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

KSTATUS
HlpOmap3InterruptDescribeLines (
    VOID
    );

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MPU_REGISTERS {
    MpuSystemConfiguration    = 0x4,  // SYSCONFIG
    MpuSystemStatus           = 0x5,  // SYSSTATUS
    MpuActiveIrq              = 0x10, // SIR_IRQ
    MpuActiveFiq              = 0x11, // SIR_FIQ
    MpuControl                = 0x12, // CONTROL
    MpuProtection             = 0x13, // PROTECTION
    MpuIdle                   = 0x14, // IDLE
    MpuIrqPriority            = 0x18, // IRQ_PRIORITY
    MpuFiqPriority            = 0x19, // FIQ_PRIORITY
    MpuCurrentPriority        = 0x1A, // THRESHOLD
    MpuRawInterruptStatus     = 0x20, // ITR (+0x20 * n)
    MpuMask                   = 0x21, // MIR (+0x20 * n)
    MpuMaskClear              = 0x22, // MIR_CLEAR (+0x20 * n)
    MpuMaskSet                = 0x23, // MIR_SET (+0x20 * n)
    MpuSoftwareInterruptSet   = 0x24, // ISR_SET (+0x20 * n)
    MpuSoftwareInterruptClear = 0x25, // ISR_CLEAR (+0x20 * n)
    MpuIrqStatus              = 0x26, // PENDING_IRQ (+0x20 * n)
    MpuFiqStatus              = 0x27, // PENDING_FIQ (+0x20 * n)
    MpuInterrupt              = 0x40  // ILR[96]
} MPU_REGISTERS, *PMPU_REGISTERS;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the virtual address of the interrupt controller.
//

PVOID HlOmap3InterruptController = NULL;

INTERRUPT_FUNCTION_TABLE HlOmap3InterruptFunctionTable = {
    HlpOmap3InterruptInitializeIoUnit,
    HlpOmap3InterruptSetLineState,
    HlpOmap3InterruptMaskLine,
    HlpOmap3InterruptBegin,
    NULL,
    HlpOmap3InterruptEndOfInterrupt,
    HlpOmap3InterruptRequestInterrupt,
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
HlpOmap3InterruptModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the OMAP3 Interrupt hardware
    module. Its role is to detect and report the prescense of an Integrator/CP
    interrupt controller.

Arguments:

    None.

Return Value:

    None.

--*/

{

    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    KSTATUS Status;

    //
    // Attempt to find the OMAP3 ACPI table.
    //

    HlOmap3Table = HlGetAcpiTable(OMAP3_SIGNATURE, NULL);
    if (HlOmap3Table == NULL) {
        goto Omap3InterruptModuleEntryEnd;
    }

    //
    // Zero out the controller description.
    //

    RtlZeroMemory(&NewController, sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));
    if (HlOmap3Table->InterruptControllerPhysicalAddress != 0) {

        //
        // Initialize the new controller structure.
        //

        NewController.TableVersion = INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;
        RtlCopyMemory(&(NewController.FunctionTable),
                      &HlOmap3InterruptFunctionTable,
                      sizeof(INTERRUPT_FUNCTION_TABLE));

        NewController.Context = NULL;
        NewController.Identifier = 0;
        NewController.ProcessorCount = 0;
        NewController.PriorityCount = OMAP3_INTERRUPT_PRIORITY_COUNT;

        //
        // Register the controller with the system.
        //

        Status = HlRegisterHardware(HardwareModuleInterruptController,
                                    &NewController);

        if (!KSUCCESS(Status)) {
            goto Omap3InterruptModuleEntryEnd;
        }
    }

Omap3InterruptModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpOmap3InterruptInitializeIoUnit (
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

    KSTATUS Status;

    if (HlOmap3InterruptController == NULL) {
        HlOmap3InterruptController = HlMapPhysicalAddress(
                              HlOmap3Table->InterruptControllerPhysicalAddress,
                              OMAP3_INTERRUPT_CONTROLLER_SIZE,
                              TRUE);

        if (HlOmap3InterruptController == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CpInterruptInitializeIoUnitEnd;
        }

        //
        // Describe the interrupt lines on this controller.
        //

        Status = HlpOmap3InterruptDescribeLines();
        if (!KSUCCESS(Status)) {
            goto CpInterruptInitializeIoUnitEnd;
        }
    }

    //
    // Disable all interrupts on the controller.
    //

    WRITE_INTERRUPT_REGISTER(MpuMaskSet + (0 * 8), 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(MpuMaskSet + (1 * 8), 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(MpuMaskSet + (2 * 8), 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(MpuCurrentPriority,
                             OMAP3_INTERRUPT_PRIORITY_COUNT);

    //
    // Reset both interrupt lines and set the new agreement.
    //

    WRITE_INTERRUPT_REGISTER(MpuControl, 3);
    Status = STATUS_SUCCESS;

CpInterruptInitializeIoUnitEnd:
    return Status;
}

INTERRUPT_CAUSE
HlpOmap3InterruptBegin (
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

    //
    // Get the currently asserting line. If it's a spurious interrupt, return
    // immediately.
    //

    ActiveIrq = READ_INTERRUPT_REGISTER(MpuActiveIrq);
    if ((ActiveIrq & MPU_SPURIOUS_INTERRUPT_MASK) != 0) {
        return InterruptCauseSpuriousInterrupt;
    }

    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->U.Local.Controller = 0;
    FiringLine->U.Local.Line = ActiveIrq;

    //
    // Save the old priority into the magic candy, and then set the priority
    // to the priority of the interrupting source.
    //

    *MagicCandy = READ_INTERRUPT_REGISTER(MpuCurrentPriority);
    ActiveIrqPriority = READ_INTERRUPT_REGISTER(MpuIrqPriority);
    WRITE_INTERRUPT_REGISTER(MpuCurrentPriority, ActiveIrqPriority);

    //
    // Write the New IRQ Agreement bit so that additional interrupts of higher
    // priority can come in.
    //

    WRITE_INTERRUPT_REGISTER(MpuControl, MPU_CONTROL_NEW_IRQ_AGREEMENT);
    return InterruptCauseLineFired;
}

VOID
HlpOmap3InterruptEndOfInterrupt (
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

    //
    // The magic candy value contained the priority register when this interrupt
    // began. Restore that value.
    //

    WRITE_INTERRUPT_REGISTER(MpuCurrentPriority, MagicCandy);
    return;
}

KSTATUS
HlpOmap3InterruptRequestInterrupt (
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
HlpOmap3InterruptSetLineState (
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

    ULONG InterruptIndex;
    ULONG InterruptOffset;
    KSTATUS Status;
    ULONG Value;

    if ((Line->Type != InterruptLineControllerSpecified) ||
        (Line->U.Local.Controller != 0) ||
        (Line->U.Local.Line >= OMAP3_INTERRUPT_LINE_COUNT)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Omap3InterruptSetLineStateEnd;
    }

    if ((State->Output.Type != InterruptLineControllerSpecified) ||
        (State->Output.U.Local.Controller != INTERRUPT_CPU_IDENTIFIER) ||
        (State->Output.U.Local.Line != INTERRUPT_CPU_IRQ_PIN)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Omap3InterruptSetLineStateEnd;
    }

    InterruptOffset = Line->U.Local.Line / 32;
    InterruptIndex = Line->U.Local.Line - (InterruptOffset * 32);

    //
    // To disable, set the interrupt mask and clean the interrupt line.
    //

    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
        WRITE_INTERRUPT_REGISTER(MpuMaskSet + (8 * InterruptOffset),
                                 1 << InterruptIndex);

        WRITE_INTERRUPT_REGISTER(MpuInterrupt + Line->U.Local.Line, 0);

    //
    // To enable, write the interrupt configuration and routing into the
    // controller.
    //

    } else {
        Value = (OMAP3_INTERRUPT_PRIORITY_COUNT - State->HardwarePriority) + 1;
        Value = Value << MPU_INTERRUPT_PRIORITY_SHIFT;
        WRITE_INTERRUPT_REGISTER(MpuInterrupt + Line->U.Local.Line, Value);
        WRITE_INTERRUPT_REGISTER(MpuMaskClear + (8 * InterruptOffset),
                                 1 << InterruptIndex);
    }

    Status = STATUS_SUCCESS;

Omap3InterruptSetLineStateEnd:
    return Status;
}

VOID
HlpOmap3InterruptMaskLine (
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

    ULONG InterruptIndex;
    ULONG InterruptOffset;
    MPU_REGISTERS Register;

    InterruptOffset = Line->U.Local.Line / 32;
    InterruptIndex = Line->U.Local.Line - (InterruptOffset * 32);

    //
    // To disable, set the interrupt mask and clean the interrupt line.
    //

    if (Enable == FALSE) {
        Register = MpuMaskSet;

    } else {
        Register = MpuMaskClear;
    }

    WRITE_INTERRUPT_REGISTER(Register + (8 * InterruptOffset),
                             1 << InterruptIndex);

    return;
}

KSTATUS
HlpOmap3InterruptDescribeLines (
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
    // Describe the normal lines on the OMAP3.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.Controller = 0;
    Lines.LineStart = 0;
    Lines.LineEnd = OMAP3_INTERRUPT_LINE_COUNT;
    Lines.Gsi = HlOmap3Table->InterruptControllerGsiBase;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto Omap3InterruptDescribeLinesEnd;
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
        goto Omap3InterruptDescribeLinesEnd;
    }

Omap3InterruptDescribeLinesEnd:
    return Status;
}


/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    b2709int.c

Abstract:

    This module implements support for the BCM2709 interrupt controller.

Author:

    Chris Stevens 24-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "bcm2709.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the flags for the basic interrupts.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_TIMER            0x00000001
#define BCM2709_INTERRUPT_IRQ_BASIC_MAILBOX          0x00000002
#define BCM2709_INTERRUPT_IRQ_BASIC_DOORBELL0        0x00000004
#define BCM2709_INTERRUPT_IRQ_BASIC_DOORBELL1        0x00000008
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU0_HALTED      0x00000010
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU1_HALTED      0x00000020
#define BCM2709_INTERRUPT_IRQ_BASIC_ILLEGAL_ACCESS_1 0x00000040
#define BCM2709_INTERRUPT_IRQ_BASIC_ILLEGAL_ACCESS_0 0x00000080

#define BCM2709_INTERRUPT_IRQ_BASIC_MASK             0x000000FF

//
// Define the flags for the GPU interrupts included in the basic pending status
// register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_7            0x00000400
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_9            0x00000800
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_10           0x00001000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_18           0x00002000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_19           0x00004000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_53           0x00008000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_54           0x00010000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_55           0x00020000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_56           0x00040000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_57           0x00080000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_62           0x00100000

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_MASK         0x001FFC00

//
// Define the number of bits to shift in order to get to the GPU bits in the
// basic pending register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_SHIFT 10

//
// Define the number of GPU registers whose pending status is expressed in the
// basic pending status register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_COUNT 11

//
// Define the flags that signify that one of the normal pending status
// registers has a pending interrupt.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_1        0x00000100
#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_2        0x00000200

#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_MASK     0x00000300

//
// Define the number of GPU interrupt lines on the BCM2709.
//

#define BCM2709_INTERRUPT_GPU_LINE_COUNT 64

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from the BCM2709 interrupt controller. The parameter should
// be a BCM2709_INTERRUPT_REGISTER value.
//

#define READ_INTERRUPT_REGISTER(_Register)   \
    HlBcm2709KernelServices->ReadRegister32( \
                            (PULONG)HlBcm2709InterruptController + (_Register))

//
// This macro writes to the BCM2709 interrupt controller. _Register should be a
// BCM2709_INTERRUPT_REGISTER value and _Value should be a ULONG.
//

#define WRITE_INTERRUPT_REGISTER(_Register, _Value)                           \
    HlBcm2709KernelServices->WriteRegister32(                                 \
                          (PULONG)HlBcm2709InterruptController + (_Register), \
                          (_Value))

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the offsets to interrupt controller registers, in ULONGs.
//

typedef enum _BCM2709_INTERRUPT_REGISTER {
    Bcm2709InterruptIrqPendingBasic = 0,
    Bcm2709InterruptIrqPending1     = 1,
    Bcm2709InterruptIrqPending2     = 2,
    Bcm2709InterruptFiqControl      = 3,
    Bcm2709InterruptIrqEnable1      = 4,
    Bcm2709InterruptIrqEnable2      = 5,
    Bcm2709InterruptIrqEnableBasic  = 6,
    Bcm2709InterruptIrqDisable1     = 7,
    Bcm2709InterruptIrqDisable2     = 8,
    Bcm2709InterruptIrqDisableBasic = 9,
    Bcm2709InterruptSize            = 0x28
} BCM2709_INTERRUPT_REGISTER, *PBCM2709_INTERRUPT_REGISTER;

//
// Define the interrupt lines for the non GPU interrupts.
//

typedef enum _BCM2709_CPU_INTERRUPT_LINE {
    Bcm2709InterruptArmTimer       = 64,
    Bcm2709InterruptArmMailbox     = 65,
    Bcm2709InterruptArmDoorbell0   = 66,
    Bcm2709InterruptArmDoorbell1   = 67,
    Bcm2709InterruptGpu0Halted     = 68,
    Bcm2709InterruptGpu1Halted     = 69,
    Bcm2709InterruptIllegalAccess1 = 70,
    Bcm2709InterruptIllegalAccess0 = 71,
    Bcm2709InterruptLineCount      = 72
} BCM2709_CPU_INTERRUPT_LINE, *PBCM2709_CPU_INTERRUPT_LINE;

/*++

Structure Description:

    This structure defines an interrupt priority level.

Members:

    IrqMaskBasic - Stores the mask for all basic interrupts that operate at the
        priority level.

    IrqMask1 - Stores the mask for all register 1 interrupts that operate at
        the priority level.

    IrqMask2 - Stores the mask for all register 2 interrupts that operate at
        the priority level.

--*/

typedef struct _BCM2709_INTERRUPT_PRIORITY_LEVEL {
    ULONG IrqMaskBasic;
    ULONG IrqMask1;
    ULONG IrqMask2;
} BCM2709_INTERRUPT_PRIORITY_LEVEL, *PBCM2709_INTERRUPT_PRIORITY_LEVEL;

/*++

Structure Description:

    This structure defines the internal data for an BCM2709 interrupt
    controller.

Members:

    LinePriority - Stores the priority level for each interrupt line.

    PriorityLevel - Stores an array of priority level information.

--*/

typedef struct _BCM2709_INTERRUPT_CONTROLLER {
    RUNLEVEL LinePriority[Bcm2709InterruptLineCount];
    BCM2709_INTERRUPT_PRIORITY_LEVEL PriorityLevel[MaxRunLevel];
} BCM2709_INTERRUPT_CONTROLLER, *PBCM2709_INTERRUPT_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpBcm2709InterruptInitializeIoUnit (
    PVOID Context
    );

INTERRUPT_CAUSE
HlpBcm2709InterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
HlpBcm2709InterruptEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

KSTATUS
HlpBcm2709InterruptRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

KSTATUS
HlpBcm2709InterruptSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State
    );

VOID
HlpBcm2709InterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

KSTATUS
HlpBcm2709InterruptDescribeLines (
    VOID
    );
//
// -------------------------------------------------------------------- Globals
//

//
// Store the virtual address of the mapped interrupt controller.
//

PVOID HlBcm2709InterruptController = NULL;

//
// Store a pointer to the provided hardware layer services.
//

PHARDWARE_MODULE_KERNEL_SERVICES HlBcm2709KernelServices = NULL;

//
// Store a pointer to the BCM2709 ACPI Table.
//

PBCM2709_TABLE HlBcm2709Table = NULL;

//
// Store a table that tracks which GPU IRQs are in the basic pending status
// register.
//

ULONG
HlBcm2709InterruptIrqBasicGpuTable[BCM2709_INTERRUPT_IRQ_BASIC_GPU_COUNT] = {
    7,
    9,
    10,
    18,
    19,
    53,
    54,
    55,
    56,
    57,
    62
};

//
// Define the interrupt function table template.
//

INTERRUPT_FUNCTION_TABLE HlBcm2709InterruptFunctionTable = {
    HlpBcm2709InterruptInitializeIoUnit,
    HlpBcm2709InterruptSetLineState,
    HlpBcm2709InterruptMaskLine,
    HlpBcm2709InterruptBegin,
    NULL,
    HlpBcm2709InterruptEndOfInterrupt,
    HlpBcm2709InterruptRequestInterrupt,
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
HlpBcm2709InterruptModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    )

/*++

Routine Description:

    This routine is the entry point for the APIC hardware module. Its role is to
    detect and report the prescense of an APIC.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    PBCM2709_INTERRUPT_CONTROLLER Context;
    ULONG Index;
    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    KSTATUS Status;

    HlBcm2709Table = Services->GetAcpiTable(BCM2709_SIGNATURE, NULL);
    if (HlBcm2709Table == NULL) {
        goto Bcm2709InterruptModuleEntryEnd;
    }

    HlBcm2709KernelServices = Services;

    //
    // Allocate the interrupt controller context.
    //

    Context = Services->AllocateMemory(sizeof(BCM2709_INTERRUPT_CONTROLLER),
                                       BCM2709_ALLOCATION_TAG,
                                       FALSE,
                                       NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709InterruptModuleEntryEnd;
    }

    RtlZeroMemory(Context, sizeof(BCM2709_INTERRUPT_CONTROLLER));
    for (Index = 0; Index < Bcm2709InterruptLineCount; Index += 1) {
        Context->LinePriority[Index] = MaxRunLevel;
    }

    //
    // Zero out the controller description.
    //

    Services->ZeroMemory(&NewController,
                         sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));

    NewController.TableVersion = INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;
    HlBcm2709KernelServices->CopyMemory(&(NewController.FunctionTable),
                                        &HlBcm2709InterruptFunctionTable,
                                        sizeof(INTERRUPT_FUNCTION_TABLE));

    NewController.Context = Context;
    NewController.Identifier = 0;
    NewController.ProcessorCount = 0;
    NewController.PriorityCount = 0;

    //
    // Register the controller with the system.
    //

    Status = Services->Register(HardwareModuleInterruptController,
                                &NewController);

    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptModuleEntryEnd;
    }

Bcm2709InterruptModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpBcm2709InterruptInitializeIoUnit (
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

    PVOID InterruptController;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    if (HlBcm2709InterruptController == NULL) {
        PhysicalAddress = HlBcm2709Table->InterruptControllerPhysicalAddress;
        InterruptController = HlBcm2709KernelServices->MapPhysicalAddress(
                                                          PhysicalAddress,
                                                          Bcm2709InterruptSize,
                                                          TRUE);

        if (InterruptController == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Bcm2709InterruptInitializeIoUnitEnd;
        }

        HlBcm2709InterruptController = InterruptController;
        Status = HlpBcm2709InterruptDescribeLines();
        if (!KSUCCESS(Status)) {
            goto Bcm2709InterruptInitializeIoUnitEnd;
        }
    }

    //
    // Disable all FIQ and IRQ lines.
    //

    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable1, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable2, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisableBasic, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptFiqControl, 0);
    Status = STATUS_SUCCESS;

Bcm2709InterruptInitializeIoUnitEnd:
    return Status;
}

INTERRUPT_CAUSE
HlpBcm2709InterruptBegin (
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

    ULONG Base;
    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG Index;
    RUNLEVEL Priority;
    PBCM2709_INTERRUPT_PRIORITY_LEVEL PriorityLevel;
    ULONG Status;

    //
    // Determine which interrupt fired based on the pending status.
    //

    Status = READ_INTERRUPT_REGISTER(Bcm2709InterruptIrqPendingBasic);
    if (Status == 0) {
        return InterruptCauseNoInterruptHere;
    }

    //
    // If this is a basic interrupt, then determine which line fired based on
    // the bit set.
    //

    if ((Status & BCM2709_INTERRUPT_IRQ_BASIC_MASK) != 0) {
        Index = 0;
        while ((Status & 0x1) == 0) {
            Status = Status >> 1;
            Index += 1;
        }

        Index += Bcm2709InterruptArmTimer;

    //
    // If this is a GPU interrupt that gets set in the basic pending status
    // register, then check which bit is set. The pending 1 and 2 bits do not
    // get set for these interrupts.
    //

    } else if ((Status & BCM2709_INTERRUPT_IRQ_BASIC_GPU_MASK) != 0) {
        Status = Status >> BCM2709_INTERRUPT_IRQ_BASIC_GPU_SHIFT;
        Index = 0;
        while ((Status & 0x1) == 0) {
            Status = Status >> 1;
            Index += 1;
        }

        ASSERT(Index < BCM2709_INTERRUPT_IRQ_BASIC_GPU_COUNT);

        Index = HlBcm2709InterruptIrqBasicGpuTable[Index];

    } else {

        ASSERT((Status & BCM2709_INTERRUPT_IRQ_BASIC_PENDING_MASK) != 0);

        if ((Status & BCM2709_INTERRUPT_IRQ_BASIC_PENDING_1) != 0) {
            Status = READ_INTERRUPT_REGISTER(Bcm2709InterruptIrqPending1);
            Base = 0;

        } else {
            Status = READ_INTERRUPT_REGISTER(Bcm2709InterruptIrqPending2);
            Base = 32;
        }

        Index = 0;
        while ((Status & 0x1) == 0) {
            Status = Status >> 1;
            Index += 1;
        }

        Index += Base;
    }

    //
    // Disable all interrupts at this priority level.
    //

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    Priority = Controller->LinePriority[Index];

    ASSERT(Priority != MaxRunLevel);

    PriorityLevel = &(Controller->PriorityLevel[Priority]);
    if (PriorityLevel->IrqMaskBasic != 0) {
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisableBasic,
                                 PriorityLevel->IrqMaskBasic);
    }

    if (PriorityLevel->IrqMask1 != 0) {
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable1,
                                 PriorityLevel->IrqMask1);
    }

    if (PriorityLevel->IrqMask2 != 0) {
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable2,
                                 PriorityLevel->IrqMask2);
    }

    //
    // Save the priority as the magic candy to re-enable these interrupts.
    //

    *MagicCandy = Priority;

    //
    // Return the interrupt line information.
    //

    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->Controller = 0;
    FiringLine->Line = Index;
    return InterruptCauseLineFired;
}

VOID
HlpBcm2709InterruptEndOfInterrupt (
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

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    RUNLEVEL Priority;
    PBCM2709_INTERRUPT_PRIORITY_LEVEL PriorityLevel;

    //
    // Enable all interrupts at this priority level.
    //

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    Priority = MagicCandy;
    PriorityLevel = &(Controller->PriorityLevel[Priority]);
    if (PriorityLevel->IrqMaskBasic != 0) {
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqEnableBasic,
                                 PriorityLevel->IrqMaskBasic);
    }

    if (PriorityLevel->IrqMask1 != 0) {
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqEnable1,
                                 PriorityLevel->IrqMask1);
    }

    if (PriorityLevel->IrqMask2 != 0) {
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqEnable2,
                                 PriorityLevel->IrqMask2);
    }

    return;
}

KSTATUS
HlpBcm2709InterruptRequestInterrupt (
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
HlpBcm2709InterruptSetLineState (
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

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG Index;
    RUNLEVEL Priority;
    BCM2709_INTERRUPT_PRIORITY_LEVEL PriorityLevel;
    BCM2709_INTERRUPT_REGISTER Register;
    ULONG RegisterValue;
    ULONG Shift;
    KSTATUS Status;

    if ((Line->Type != InterruptLineControllerSpecified) ||
        (Line->Controller != 0) ||
        (Line->Line >= Bcm2709InterruptLineCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Bcm2709SetLineStateEnd;
    }

    if ((State->Output.Type != InterruptLineControllerSpecified) ||
        (State->Output.Controller != INTERRUPT_CPU_IDENTIFIER) ||
        (State->Output.Line != INTERRUPT_CPU_IRQ_PIN)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Bcm2709SetLineStateEnd;
    }

    RtlZeroMemory(&PriorityLevel, sizeof(BCM2709_INTERRUPT_PRIORITY_LEVEL));

    //
    // If the line is a GPU line, then determine which of the two
    // disable/enable registers it belongs to.
    //

    if (Line->Line < BCM2709_INTERRUPT_GPU_LINE_COUNT) {
        Shift = Line->Line;
        if (Line->Line >= 32) {
            Shift -= 32;
        }

        RegisterValue = 1 << Shift;
        if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
            if (Line->Line < 32) {
                Register = Bcm2709InterruptIrqDisable1;

            } else {
                Register = Bcm2709InterruptIrqDisable2;
            }

        } else {
            if (Line->Line < 32) {
                Register = Bcm2709InterruptIrqEnable1;

            } else {
                Register = Bcm2709InterruptIrqEnable2;
            }
        }

        //
        // Set the mask in the priority level.
        //

        if (Line->Line < 32) {
            PriorityLevel.IrqMask1 |= RegisterValue;

        } else {
            PriorityLevel.IrqMask2 |= RegisterValue;
        }

    //
    // Otherwise the interrupt belongs to the basic enable and disable
    // registers.
    //

    } else {
        Shift = Line->Line - BCM2709_INTERRUPT_GPU_LINE_COUNT;
        RegisterValue = 1 << Shift;

        ASSERT((RegisterValue & BCM2709_INTERRUPT_IRQ_BASIC_MASK) != 0);

        if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
            Register = Bcm2709InterruptIrqDisableBasic;

        } else {
            Register = Bcm2709InterruptIrqEnableBasic;
        }

        //
        // Set the mask in the priority level.
        //

        PriorityLevel.IrqMaskBasic |= RegisterValue;
    }

    //
    // Determine which priority level this interrupt belongs to.
    //

    Priority = VECTOR_TO_RUN_LEVEL(State->Vector);

    //
    // If the interrupt is about to be enabled, make sure the priority mask is
    // updated first.
    //

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) != 0) {
        Controller->LinePriority[Line->Line] = Priority;
        for (Index = 0; Index <= Priority; Index += 1) {
            if (PriorityLevel.IrqMaskBasic != 0) {
                Controller->PriorityLevel[Index].IrqMaskBasic |=
                                                    PriorityLevel.IrqMaskBasic;
            }

            if (PriorityLevel.IrqMask1 != 0) {
                Controller->PriorityLevel[Index].IrqMask1 |=
                                                        PriorityLevel.IrqMask1;
            }

            if (PriorityLevel.IrqMask2 != 0) {
                Controller->PriorityLevel[Index].IrqMask2 |=
                                                        PriorityLevel.IrqMask2;
            }
        }
    }

    //
    // Change the state of the interrupt based on the register and the value
    // determined above.
    //

    WRITE_INTERRUPT_REGISTER(Register, RegisterValue);

    //
    // If the interrupt was just disabled, make sure the priority mask is
    // updated after.
    //

    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {

        ASSERT(Controller->LinePriority[Line->Line] == Priority);

        for (Index = 0; Index <= Priority; Index += 1) {
            if (PriorityLevel.IrqMaskBasic != 0) {
                Controller->PriorityLevel[Index].IrqMaskBasic &=
                                                   ~PriorityLevel.IrqMaskBasic;
            }

            if (PriorityLevel.IrqMask1 != 0) {
                Controller->PriorityLevel[Index].IrqMask1 &=
                                                       ~PriorityLevel.IrqMask1;
            }

            if (PriorityLevel.IrqMask2 != 0) {
                Controller->PriorityLevel[Index].IrqMask2 &=
                                                       ~PriorityLevel.IrqMask2;
            }
        }

        Controller->LinePriority[Line->Line] = 0;
    }

    Status = STATUS_SUCCESS;

Bcm2709SetLineStateEnd:
    return Status;
}

VOID
HlpBcm2709InterruptMaskLine (
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

    BCM2709_INTERRUPT_REGISTER Register;
    ULONG RegisterValue;
    ULONG Shift;

    //
    // If the line is a GPU line, then determine which of the two
    // disable/enable registers it belongs to.
    //

    if (Line->Line < BCM2709_INTERRUPT_GPU_LINE_COUNT) {
        Shift = Line->Line;
        if (Line->Line >= 32) {
            Shift -= 32;
        }

        RegisterValue = 1 << Shift;
        if (Enable == FALSE) {
            if (Line->Line < 32) {
                Register = Bcm2709InterruptIrqDisable1;

            } else {
                Register = Bcm2709InterruptIrqDisable2;
            }

        } else {
            if (Line->Line < 32) {
                Register = Bcm2709InterruptIrqEnable1;

            } else {
                Register = Bcm2709InterruptIrqEnable2;
            }
        }

    //
    // Otherwise the interrupt belongs to the basic enable and disable
    // registers.
    //

    } else {
        Shift = Line->Line - BCM2709_INTERRUPT_GPU_LINE_COUNT;
        RegisterValue = 1 << Shift;

        ASSERT((RegisterValue & BCM2709_INTERRUPT_IRQ_BASIC_MASK) != 0);

        if (Enable == FALSE) {
            Register = Bcm2709InterruptIrqDisableBasic;

        } else {
            Register = Bcm2709InterruptIrqEnableBasic;
        }
    }

    WRITE_INTERRUPT_REGISTER(Register, RegisterValue);
    return;
}

KSTATUS
HlpBcm2709InterruptDescribeLines (
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

    HlBcm2709KernelServices->ZeroMemory(&Lines,
                                        sizeof(INTERRUPT_LINES_DESCRIPTION));

    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;

    //
    // Describe the normal lines on the BCM2709.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.Controller = 0;
    Lines.LineStart = 0;
    Lines.LineEnd = Bcm2709InterruptLineCount;
    Lines.Gsi = HlBcm2709Table->InterruptControllerGsiBase;
    Status = HlBcm2709KernelServices->Register(HardwareModuleInterruptLines,
                                               &Lines);

    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptDescribeLinesEnd;
    }

    //
    // Register the output lines.
    //

    Lines.Type = InterruptLinesOutput;
    Lines.OutputControllerIdentifier = INTERRUPT_CPU_IDENTIFIER;
    Lines.LineStart = INTERRUPT_ARM_MIN_CPU_LINE;
    Lines.LineEnd = INTERRUPT_ARM_MAX_CPU_LINE;
    Status = HlBcm2709KernelServices->Register(HardwareModuleInterruptLines,
                                               &Lines);

    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptDescribeLinesEnd;
    }

Bcm2709InterruptDescribeLinesEnd:
    return Status;
}


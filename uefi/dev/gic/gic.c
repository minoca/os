/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gic.c

Abstract:

    This module implements support for the ARM Generic Interrupt Controller.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "dev/gic.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Define register access macros to the distributor and CPU interface.
//

#define READ_GIC_DISTRIBUTOR(_Context, _Register)                              \
    EfiReadRegister32((UINT32 *)((_Context)->DistributorBase + (_Register)))

#define WRITE_GIC_DISTRIBUTOR(_Context, _Register, _Value)                     \
    EfiWriteRegister32((UINT32 *)((_Context)->DistributorBase + (_Register)),  \
                       (_Value))

#define READ_GIC_DISTRIBUTOR_BYTE(_Context, _Register)                         \
    EfiReadRegister8((UINT8 *)((_Context)->DistributorBase + (_Register))

#define WRITE_GIC_DISTRIBUTOR_BYTE(_Context, _Register, _Value)                \
    EfiWriteRegister8((UINT8 *)((_Context)->DistributorBase + (_Register)),    \
                      (_Value))

#define READ_GIC_CPU_INTERFACE(_Context, _Register)                            \
    EfiReadRegister32((UINT8 *)((_Context)->CpuInterfaceBase + (_Register)))

#define WRITE_GIC_CPU_INTERFACE(_Context, _Register, _Value)                   \
    EfiWriteRegister32((UINT8 *)((_Context)->CpuInterfaceBase + (_Register)),  \
                       (_Value))

#define READ_GIC_CPU_INTERFACE_BYTE(_Context, _Register)                       \
    EfiReadRegister8((UINT8 *)((_Context)->CpuInterfaceBase + (_Register)))

#define WRITE_GIC_CPU_INTERFACE_BYTE(_Context, _Register, _Value)              \
    EfiWriteRegister8((UINT8 *)((_Context)->CpuInterfaceBase + (_Register)),   \
                      (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of software interrupt (SGI) lines.
//

#define GIC_SOFTWARE_INTERRUPT_LINE_COUNT 16

//
// Define the maximum number of lines a GIC can have.
//

#define GIC_MAX_LINES 1024

//
// Define the spurious line.
//

#define GIC_SPURIOUS_LINE 1023

//
// GIC Distributor register definitions.
//

//
// Define distributor Control register bits.
//

#define GIC_DISTRIBUTOR_CONTROL_ENABLE 0x1

//
// Define register bits of the distributor type register.
//

#define GIC_DISTRIBUTOR_TYPE_LINE_COUNT_MASK 0x1F

//
// Define register bits of the software interrupt register.
//

#define GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_ALL_BUT_SELF_SHORTHAND 0x01000000
#define GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_SELF_SHORTHAND 0x02000000
#define GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_TARGET_SHIFT 16

//
// Define register bits of the interrupt configuration register.
//

#define GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_EDGE_TRIGGERED 0x2
#define GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_N_TO_N 0x0
#define GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_1_TO_N 0x1
#define GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_MASK 0x3

//
// GIC CPU Interface register definitions.
//

//
// Define the control register bit definitions.
//

#define GIC_CPU_INTERFACE_CONTROL_ENABLE 0x1

//
// Define register definitions for the CPU interface binary point register.
// All GICs must support a binary point of at least 3, meaning there are 4 bits
// for the priority group, and therefore 16 unique priority levels.
//

#define GIC_CPU_INTERFACE_BINARY_POINT_MINIMUM 3

//
// Define register definitions for the interrupt acknowledge register.
//

#define GIC_CPU_INTERFACE_ACKNOWLEDGE_LINE_MASK 0x3FF

//
// Define the priority assigned to all enabled interrupts.
//

#define EFI_GIC_INTERRUPT_PRIORITY 0x80
#define EFI_GIC_LOW_PRIORITY 0xF0

//
// ------------------------------------------------------ Data Type Definitions
//
//
// Define the GIC Distributor register offsets, in bytes.
//

typedef enum _GIC_DISTRIBUTOR_REGISTER {
    GicDistributorControl                       = 0x000, // GICD_CTLR
    GicDistributorType                          = 0x004, // GICD_TYPER
    GicDistributorImplementor                   = 0x008, // GICD_IIDR
    GicDistributorGroup                         = 0x080, // GICD_IGROUPRn
    GicDistributorEnableSet                     = 0x100, // GICD_ISENABLERn
    GicDistributorEnableClear                   = 0x180, // GICD_ICENABLERn
    GicDistributorPendingSet                    = 0x200, // GICD_ISPENDRn
    GicDistributorPendingClear                  = 0x280, // GICD_ICPENDRn
    GicDistributorActiveSet                     = 0x300, // GICD_ISACTIVERn
    GicDistributorActiveClear                   = 0x380, // GICD_ICACTIVERn
    GicDistributorPriority                      = 0x400, // GICD_IPRIORITYRn
    GicDistributorInterruptTarget               = 0x800, // GICD_ITARGETSRn
    GicDistributorInterruptConfiguration        = 0xC00, // GICD_ICFGRn
    GicDistributorNonSecureAccessControl        = 0xE00, // GICD_NSACRn
    GicDistributorSoftwareInterrupt             = 0xF00, // GICD_SGIR
    GicDistributorSoftwareInterruptPendingClear = 0xF10, // GICD_CPENDSGIRn
    GicDistributorSoftwareInterruptPendingSet   = 0xF20, // GICD_SPENDSSGIRn
} GIC_DISTRIBUTOR_REGISTER, *PGIC_DISTRIBUTOR_REGISTER;

//
// Define the GIC CPU Interface register offsets, in bytes.
//

typedef enum _GIC_CPU_INTERFACE_REGISTER {
    GicCpuInterfaceControl                       = 0x00, // GICC_CTLR
    GicCpuInterfacePriorityMask                  = 0x04, // GICC_PMR
    GicCpuInterfaceBinaryPoint                   = 0x08, // GICC_BPR
    GicCpuInterfaceInterruptAcknowledge          = 0x0C, // GICC_IAR
    GicCpuInterfaceEndOfInterrupt                = 0x10, // GICC_EOIR
    GicCpuInterfaceRunningPriority               = 0x14, // GICC_RPR
    GicCpuInterfaceHighestPendingPriority        = 0x18, // GICC_HPPIR
    GicCpuInterfaceAliasedBinaryPoint            = 0x1C, // GICC_ABPR,
    GicCpuInterfaceAliasedInterruptAcknowledge   = 0x20, // GICC_AIAR
    GicCpuInterfaceAliasedEndOfInterrupt         = 0x24, // GICC_AEOIR
    GicCpuInterfaceAliasedHighestPendingPriority = 0x28, // GICC_AHPPIR
    GicCpuInterfaceActivePriority                = 0xD0, // GICC_APRn
    GicCpuInterfaceNonSecureActivePriority       = 0xE0, // GICC_NSAPRn
    GicCpuInterfaceIdentification                = 0xFC, // GICC_IIDR
    GicCpuInterfaceDeactivateInterrupt           = 0x1000 // GICC_DIR
} GIC_CPU_INTERFACE_REGISTER, *PGIC_CPU_INTERFACE_REGISTER;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipGicInitialize (
    PGIC_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a Generic Interrupt Controller. It enables the
    controller and masks all interrupt lines.

Arguments:

    Context - Supplies the pointer to the controller's context. The base must
        be filled in by the caller, and the rest must be zeroed out by the
        caller.

Return Value:

    EFI Status code.

--*/

{

    UINT32 BlockIndex;
    UINT32 LineCountField;
    EFI_STATUS Status;

    if ((Context->DistributorBase == NULL) ||
        (Context->CpuInterfaceBase == NULL)) {

        Status = EFI_INVALID_PARAMETER;
        goto GicInitializeEnd;
    }

    //
    // Determine the maximum number of lines that this controller may have.
    //

    LineCountField = READ_GIC_DISTRIBUTOR(Context, GicDistributorType) &
                     GIC_DISTRIBUTOR_TYPE_LINE_COUNT_MASK;

    Context->MaxLines = 32 * (LineCountField + 1);

    //
    // Mask every interrupt in the distributor.
    //

    for (BlockIndex = 0;
         BlockIndex < Context->MaxLines / 32;
         BlockIndex += 1) {

        WRITE_GIC_DISTRIBUTOR(Context,
                              GicDistributorEnableClear + (4 * BlockIndex),
                              0xFFFFFFFF);
    }

    //
    // Enable all the software generated interrupts (lines 0-16).
    //

    WRITE_GIC_DISTRIBUTOR(Context, GicDistributorEnableSet, 0x0000FFFF);

    //
    // Enable the GIC distributor.
    //

    WRITE_GIC_DISTRIBUTOR(Context,
                          GicDistributorControl,
                          GIC_DISTRIBUTOR_CONTROL_ENABLE);

    //
    // Set the binary point register to define where the priority group ends
    // and the subgroup begins. Initialize it to the most conservative value
    // that all implementations must support.
    //

    WRITE_GIC_CPU_INTERFACE(Context,
                            GicCpuInterfaceBinaryPoint,
                            GIC_CPU_INTERFACE_BINARY_POINT_MINIMUM);

    //
    // Set the running priority to its lowest value.
    //

    WRITE_GIC_CPU_INTERFACE(Context,
                            GicCpuInterfacePriorityMask,
                            EFI_GIC_LOW_PRIORITY);

    //
    // Enable this CPU interface.
    //

    WRITE_GIC_CPU_INTERFACE(Context,
                            GicCpuInterfaceControl,
                            GIC_CPU_INTERFACE_CONTROL_ENABLE);

    Status = EFI_SUCCESS;

GicInitializeEnd:
    return Status;
}

VOID
EfipGicBeginInterrupt (
    PGIC_CONTEXT Context,
    UINT32 *InterruptNumber,
    VOID **InterruptContext
    )

/*++

Routine Description:

    This routine is called when an interrupts comes in. This routine determines
    the interrupt source.

Arguments:

    Context - Supplies a pointer to the interrupt controller context.

    InterruptNumber - Supplies a pointer where interrupt line number will be
        returned.

    InterruptContext - Supplies a pointer where the platform can store a
        pointer's worth of context that will be passed back when ending the
        interrupt.

Return Value:

    None.

--*/

{

    UINT32 AcknowledgeRegister;

    //
    // Read the interrupt acknowledge register, which accepts the highest
    // priority interrupt (marking it from pending to active). save this in the
    // context area to know what to EOI.
    //

    AcknowledgeRegister =
                   READ_GIC_CPU_INTERFACE(Context,
                                          GicCpuInterfaceInterruptAcknowledge);

    *InterruptNumber =
                 AcknowledgeRegister & GIC_CPU_INTERFACE_ACKNOWLEDGE_LINE_MASK;

    *InterruptContext = (VOID *)(UINTN)AcknowledgeRegister;
    return;
}

VOID
EfipGicEndInterrupt (
    PGIC_CONTEXT Context,
    UINT32 InterruptNumber,
    VOID *InterruptContext
    )

/*++

Routine Description:

    This routine is called to finish handling of a platform interrupt. This is
    where the End-Of-Interrupt would get sent to the interrupt controller.

Arguments:

    Context - Supplies a pointer to the interrupt controller context.

    InterruptNumber - Supplies the interrupt number that occurred.

    InterruptContext - Supplies the context returned by the interrupt
        controller when the interrupt began.

Return Value:

    None.

--*/

{

    //
    // Write the value put into the opaque token into the EOI register.
    //

    if (InterruptNumber != GIC_SPURIOUS_LINE) {
        WRITE_GIC_CPU_INTERFACE(Context,
                                GicCpuInterfaceEndOfInterrupt,
                                (UINT32)(UINTN)InterruptContext);
    }

    return;
}

EFI_STATUS
EfipGicSetLineState (
    PGIC_CONTEXT Context,
    UINT32 LineNumber,
    BOOLEAN Enabled,
    BOOLEAN EdgeTriggered
    )

/*++

Routine Description:

    This routine enables or disables an interrupt line.

Arguments:

    Context - Supplies the pointer to the controller's context.

    LineNumber - Supplies the line number to enable or disable.

    Enabled - Supplies a boolean indicating if the line should be enabled or
        disabled.

    EdgeTriggered - Supplies a boolean indicating if the interrupt is edge
        triggered (TRUE) or level triggered (FALSE).

Return Value:

    EFI Status code.

--*/

{

    UINT32 Configuration;
    UINT32 ConfigurationBlock;
    UINT32 ConfigurationShift;
    UINT32 LineBit;
    UINT32 LineBlock;
    EFI_STATUS Status;
    UINT8 Target;

    LineBlock = (LineNumber / 32) * 4;
    LineBit = LineNumber % 32;
    Status = EFI_SUCCESS;

    //
    // Fail if the system is trying to set a really wacky interrupt line number.
    //

    if (LineNumber >= GIC_MAX_LINES) {
        Status = EFI_INVALID_PARAMETER;
        goto GicSetLineStateEnd;
    }

    //
    // Simply clear out the line if it is being disabled.
    //

    if (Enabled == FALSE) {
        WRITE_GIC_DISTRIBUTOR(Context,
                              GicDistributorEnableClear + LineBlock,
                              1 << LineBit);

        Status = EFI_SUCCESS;
        goto GicSetLineStateEnd;
    }

    //
    // Set the priority of the requested line.
    //

    WRITE_GIC_DISTRIBUTOR_BYTE(Context,
                               GicDistributorPriority + LineNumber,
                               EFI_GIC_INTERRUPT_PRIORITY);

    //
    // The interrupt always targets the first processor.
    //

    Target = 0x01;
    WRITE_GIC_DISTRIBUTOR_BYTE(Context,
                               GicDistributorInterruptTarget + LineNumber,
                               Target);

    //
    // Set the configuration register.
    //

    ConfigurationBlock = 4 * (LineNumber / 16);
    ConfigurationShift = 2 * (LineNumber % 16);
    Configuration = READ_GIC_DISTRIBUTOR(
                    Context,
                    GicDistributorInterruptConfiguration + ConfigurationBlock);

    //
    // Mask out all the bits being set, then set the appropriate ones.
    //

    Configuration &= ~(GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_MASK <<
                       ConfigurationShift);

    if (EdgeTriggered != FALSE) {
        Configuration |=
                      GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_EDGE_TRIGGERED <<
                      ConfigurationShift;
    }

    WRITE_GIC_DISTRIBUTOR(
                     Context,
                     GicDistributorInterruptConfiguration + ConfigurationBlock,
                     Configuration);

    //
    // Enable the line.
    //

    WRITE_GIC_DISTRIBUTOR(Context,
                          GicDistributorEnableSet + LineBlock,
                          1 << LineBit);

GicSetLineStateEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//


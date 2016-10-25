/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gic.c

Abstract:

    This module implements hardware module plugin support for ARM's Generic
    Interrupt Controller (GIC) v2.

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
#include <minoca/kernel/arm.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro performs a 32-bit read from a GIC Distributor. _Controller should
// be a GIC_DISTRIBUTOR_DATA pointer, and _Register should be a
// GIC_DISTRIBUTOR_REGISTER value.
//

#define READ_GIC_DISTRIBUTOR(_Controller, _Register) \
    HlReadRegister32((_Controller)->Distributor + (_Register))

//
// This macro performs a 32-bit write to a GIC Distributor. _Controller should
// be a GIC_DISTRIBUTOR_DATA pointer, _Register should be a
// GIC_DISTRIBUTOR_REGISTER value, and _Value is a 32-bit value.
//

#define WRITE_GIC_DISTRIBUTOR(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->Distributor + (_Register), (_Value))

//
// This macro performs an 8-bit read from a GIC Distributor. _Controller should
// be a GIC_DISTRIBUTOR_DATA pointer, and _Register should be a
// GIC_DISTRIBUTOR_REGISTER value.
//

#define READ_GIC_DISTRIBUTOR_BYTE(_Controller, _Register) \
    HlReadRegister8((_Controller)->Distributor + (_Register))

//
// This macro performs an 8-bit write to a GIC Distributor. _Controller should
// be a GIC_DISTRIBUTOR_DATA pointer, _Register should be a
// GIC_DISTRIBUTOR_REGISTER value, and _Value is an 8-bit value.
//

#define WRITE_GIC_DISTRIBUTOR_BYTE(_Controller, _Register, _Value) \
    HlWriteRegister8((_Controller)->Distributor + (_Register), (_Value))

//
// This macro performs a 32-bit read from a GIC CPU Interface register.
// _Register should be a GIC_CPU_INTERFACE_REGISTER.
//

#define READ_GIC_CPU_INTERFACE(_Register) \
    HlReadRegister32(HlGicCpuInterface + (_Register))

//
// This macro performs a 32-bit write to a GIC CPU Interface register.
// _Register should be a GIC_CPU_INTERFACE_REGISTER, and _Value should be the
// 32-bit value to write.
//

#define WRITE_GIC_CPU_INTERFACE(_Register, _Value) \
    HlWriteRegister32(HlGicCpuInterface + (_Register), (_Value))

//
// This macro performs an 8-bit read from a GIC CPU Interface register.
// _Register should be a GIC_CPU_INTERFACE_REGISTER.
//

#define READ_GIC_CPU_INTERFACE_BYTE(_Register) \
    HlReadRegister8(HlGicCpuInterface + (_Register))

//
// This macro performs an 8-bit write to a GIC CPU Interface register.
// _Register should be a GIC_CPU_INTERFACE_REGISTER, and _Value should be the
// 8-bit value to write.
//

#define WRITE_GIC_CPU_INTERFACE_BYTE(_Register, _Value) \
    HlWriteRegister8(HlGicCpuInterface + (_Register), (_Value))

//
// This macro converts from a hardware priority value passed by the system to
// a priority value that can be written to the GIC registers.
//

#define SYSTEM_TO_GIC_PRIORITY(_SystemPriority) ((0xF - (_SystemPriority)) << 4)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the GIC allocation tag.
//

#define GIC_ALLOCATION_TAG 0x32434947 //'2CIG'

//
// Define the size of the GIC register blocks.
//

#define GIC_DISTRIBUTOR_SIZE 0x1000
#define GIC_CPU_INTERFACE_SIZE 0x2000

//
// Define the minimum number of unique priority levels required architecturally.
//

#define GIC_MINIMUM_PRIORITY_COUNT 16

//
// Define the number of software interrupt (SGI) lines.
//

#define GIC_SOFTWARE_INTERRUPT_LINE_COUNT 16

//
// Define the number of processor peripheral (PPI) lines.
//

#define GIC_PROCESSOR_PERIPHERAL_LINE_BASE GIC_SOFTWARE_INTERRUPT_LINE_COUNT
#define GIC_PROCESSOR_PERIPHERAL_LINE_COUNT 16

//
// Define where normal lines begin.
//

#define GIC_PROCESSOR_NORMAL_LINE_BASE \
    (GIC_PROCESSOR_PERIPHERAL_LINE_BASE + GIC_PROCESSOR_PERIPHERAL_LINE_COUNT)

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
// Define which bits of the MPIDR are valid processor ID bits for the local
// GIC.
//

#define GIC_PROCESSOR_ID_MASK 0x000000FF

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

/*++

Structure Description:

    This structure describes data internal to the GIC hardware module about
    a GIC Distributor.

Members:

    Distributor - Stores the virtual address of the mapping to the Distributor.

    PhysicalAddress - Stores the physical address of the Distributor's base.

    GsiBase - Stores the global system interrupt base of the Distributor.

    Identifier - Stores the identifier of this Distributor.

    MaxLines - Stores the maximun number of lines that this distributor has.

--*/

typedef struct _GIC_DISTRIBUTOR_DATA {
    PVOID Distributor;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG GsiBase;
    ULONG Identifier;
    ULONG MaxLines;
} GIC_DISTRIBUTOR_DATA, *PGIC_DISTRIBUTOR_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpGicEnumerateProcessors (
    PVOID Context,
    PPROCESSOR_DESCRIPTION Descriptions,
    ULONG DescriptionsBufferSize
    );

KSTATUS
HlpGicInitializeLocalUnit (
    PVOID Context,
    PULONG Identifier
    );

KSTATUS
HlpGicInitializeIoUnit (
    PVOID Context
    );

KSTATUS
HlpGicSetLocalUnitAddressing (
    PVOID Context,
    PINTERRUPT_HARDWARE_TARGET Target
    );

INTERRUPT_CAUSE
HlpGicInterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
HlpGicEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

KSTATUS
HlpGicRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

KSTATUS
HlpGicStartProcessor (
    PVOID Context,
    ULONG Identifier,
    PHYSICAL_ADDRESS JumpAddressPhysical
    );

KSTATUS
HlpGicSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

VOID
HlpGicMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

KSTATUS
HlpGicResetLocalUnit (
    VOID
    );

KSTATUS
HlpGicDescribeLines (
    PGIC_DISTRIBUTOR_DATA Controller
    );

KSTATUS
HlpGicSetupIoUnitAccess (
    PGIC_DISTRIBUTOR_DATA Controller
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the mapping for the local unit. This code assumes that
// all CPU interfaces will lie at the same physical address.
//

PVOID HlGicCpuInterface = NULL;

//
// Store a pointer to the MADT Table.
//

PMADT HlGicMadt = NULL;

//
// Define the interrupt function table template.
//

INTERRUPT_FUNCTION_TABLE HlGicFunctionTable = {
    HlpGicInitializeIoUnit,
    HlpGicSetLineState,
    HlpGicMaskLine,
    HlpGicInterruptBegin,
    NULL,
    HlpGicEndOfInterrupt,
    HlpGicRequestInterrupt,
    HlpGicEnumerateProcessors,
    HlpGicInitializeLocalUnit,
    HlpGicSetLocalUnitAddressing,
    HlpGicStartProcessor,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpGicModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the GIC hardware module. Its role is to
    detect and report the prescense of a GIC.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PMADT_GENERIC_ENTRY CurrentEntry;
    PMADT_GIC_DISTRIBUTOR Distributor;
    PGIC_DISTRIBUTOR_DATA DistributorData;
    PMADT MadtTable;
    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    ULONG ProcessorCount;
    KSTATUS Status;

    //
    // Attempt to find an MADT. If one exists, then the GIC is present.
    //

    MadtTable = HlGetAcpiTable(MADT_SIGNATURE, NULL);
    if (MadtTable == NULL) {
        goto GicModuleEntryEnd;
    }

    HlGicMadt = MadtTable;

    //
    // Zero out the controller description.
    //

    RtlZeroMemory(&NewController, sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));

    //
    // Loop through every entry in the MADT once to determine the number of
    // processors in the system.
    //

    ProcessorCount = 0;
    CurrentEntry = (PMADT_GENERIC_ENTRY)(MadtTable + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)MadtTable + MadtTable->Header.Length)) {

        if ((CurrentEntry->Type == MadtEntryTypeGic) &&
            (CurrentEntry->Length == sizeof(MADT_GIC))) {

            ProcessorCount += 1;
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

    //
    // Fail if the MADT is malformed and no processors are present.
    //

    if (ProcessorCount == 0) {
        goto GicModuleEntryEnd;
    }

    //
    // Loop through again to register all IOAPICs (GIC Distributors). Associate
    // all processors with the first GIC stumbled across.
    //

    CurrentEntry = (PMADT_GENERIC_ENTRY)(MadtTable + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)MadtTable + MadtTable->Header.Length)) {

        if ((CurrentEntry->Type == MadtEntryTypeGicDistributor) &&
            (CurrentEntry->Length == sizeof(MADT_GIC_DISTRIBUTOR))) {

            Distributor = (PMADT_GIC_DISTRIBUTOR)CurrentEntry;

            //
            // Allocate context needed for this Distributor.
            //

            DistributorData = HlAllocateMemory(sizeof(GIC_DISTRIBUTOR_DATA),
                                               GIC_ALLOCATION_TAG,
                                               FALSE,
                                               NULL);

            if (DistributorData == NULL) {
                goto GicModuleEntryEnd;
            }

            RtlZeroMemory(DistributorData, sizeof(GIC_DISTRIBUTOR_DATA));
            DistributorData->PhysicalAddress = Distributor->BaseAddress;
            DistributorData->Distributor = NULL;
            DistributorData->GsiBase = Distributor->GsiBase;
            DistributorData->Identifier = Distributor->GicId;

            //
            // Initialize the new controller structure.
            //

            NewController.TableVersion =
                                      INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;

            RtlCopyMemory(&(NewController.FunctionTable),
                          &HlGicFunctionTable,
                          sizeof(INTERRUPT_FUNCTION_TABLE));

            NewController.Context = DistributorData;
            NewController.Identifier = DistributorData->Identifier;
            NewController.ProcessorCount = ProcessorCount;
            NewController.PriorityCount = GIC_MINIMUM_PRIORITY_COUNT;
            ProcessorCount = 0;

            //
            // Register the controller with the system.
            //

            Status = HlRegisterHardware(HardwareModuleInterruptController,
                                        &NewController);

            if (!KSUCCESS(Status)) {
                goto GicModuleEntryEnd;
            }
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

GicModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpGicEnumerateProcessors (
    PVOID Context,
    PPROCESSOR_DESCRIPTION Descriptions,
    ULONG DescriptionsBufferSize
    )

/*++

Routine Description:

    This routine describes all processors.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Descriptions - Supplies a pointer to a buffer of an array of processor
        descriptions that the hardware module is expected to fill out on
        success. The number of entries in the array is the number of processors
        reported during the interrupt controller registration.

    DescriptionsBufferSize - Supplies the size of the buffer passed. The
        hardware module should fail the routine if the buffer size is smaller
        than expected, but should not fail if the buffer size is larger than
        expected.

Return Value:

    STATUS_SUCCESS on success. The Descriptions buffer will contain
        descriptions of all processors under the jurisdiction of the given
        interrupt controller.

    Other status codes on failure. The contents of the Descriptions buffer is
        undefined.

--*/

{

    PMADT_GENERIC_ENTRY CurrentEntry;
    PPROCESSOR_DESCRIPTION CurrentProcessor;
    PMADT_GIC LocalGic;
    PMADT MadtTable;
    ULONG ProcessorCount;
    KSTATUS Status;

    MadtTable = HlGicMadt;
    if (MadtTable == NULL) {
        Status = STATUS_NOT_INITIALIZED;
        goto GicEnumerateProcessorsEnd;
    }

    //
    // Loop through every entry in the MADT looking for CPU interfaces (in the
    // table as local GICs).
    //

    ProcessorCount = 0;
    CurrentProcessor = Descriptions;
    CurrentEntry = (PMADT_GENERIC_ENTRY)(MadtTable + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)MadtTable + MadtTable->Header.Length)) {

        if ((CurrentEntry->Type == MadtEntryTypeGic) &&
            (CurrentEntry->Length == sizeof(MADT_GIC))) {

            LocalGic = (PMADT_GIC)CurrentEntry;
            ProcessorCount += 1;

            //
            // Fail if the buffer is not big enough for this processor.
            //

            if (sizeof(PROCESSOR_DESCRIPTION) * ProcessorCount >
                DescriptionsBufferSize) {

                Status = STATUS_BUFFER_TOO_SMALL;
                goto GicEnumerateProcessorsEnd;
            }

            CurrentProcessor->Version = PROCESSOR_DESCRIPTION_VERSION;
            CurrentProcessor->PhysicalId = LocalGic->GicId;
            CurrentProcessor->LogicalFlatId =
                                1 << (LocalGic->GicId & GIC_PROCESSOR_ID_MASK);

            CurrentProcessor->FirmwareIdentifier = LocalGic->AcpiProcessorId;
            CurrentProcessor->Flags = 0;
            if ((LocalGic->Flags & MADT_LOCAL_GIC_FLAG_ENABLED) != 0) {
                CurrentProcessor->Flags |= PROCESSOR_DESCRIPTION_FLAG_PRESENT;
            }

            CurrentProcessor->ParkedPhysicalAddress = LocalGic->ParkedAddress;
            CurrentProcessor += 1;
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

    Status = STATUS_SUCCESS;

GicEnumerateProcessorsEnd:
    return Status;
}

KSTATUS
HlpGicInitializeLocalUnit (
    PVOID Context,
    PULONG Identifier
    )

/*++

Routine Description:

    This routine initializes the local unit of an interrupt controller. It is
    always called on the processor of the local unit to initialize.

Arguments:

    Context - Supplies the pointer to the context of the I/O unit that owns
        this processor, provided by the hardware module upon initialization.

    Identifier - Supplies a pointer where this function will return the
        identifier of the processor being initialized.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG ProcessorId;
    KSTATUS Status;

    //
    // Map the CPU Interface to virtual address space if that has not been done
    // so yet.
    //

    if (HlGicCpuInterface == NULL) {
        if (HlGicMadt == NULL) {
            Status = STATUS_NOT_INITIALIZED;
            goto GicInitializeLocalUnitEnd;
        }

        PhysicalAddress = HlGicMadt->ApicAddress;
        HlGicCpuInterface = HlMapPhysicalAddress(PhysicalAddress,
                                                 GIC_CPU_INTERFACE_SIZE,
                                                 TRUE);

        if (HlGicCpuInterface == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GicInitializeLocalUnitEnd;
        }
    }

    Status = HlpGicResetLocalUnit();
    if (!KSUCCESS(Status)) {
        goto GicInitializeLocalUnitEnd;
    }

    //
    // Set up access to the distributor at this time as well. This is needed to
    // read one of its per-CPU banked registers to determine what CPU this is.
    //

    Status = HlpGicSetupIoUnitAccess(Context);
    if (!KSUCCESS(Status)) {
        goto GicInitializeLocalUnitEnd;
    }

    ProcessorId = ArGetMultiprocessorIdRegister();
    *Identifier = ProcessorId & ARM_PROCESSOR_ID_MASK;

GicInitializeLocalUnitEnd:
    return Status;
}

KSTATUS
HlpGicInitializeIoUnit (
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

    ULONG BlockIndex;
    PGIC_DISTRIBUTOR_DATA Controller;
    KSTATUS Status;

    Controller = (PGIC_DISTRIBUTOR_DATA)Context;
    Status = HlpGicSetupIoUnitAccess(Controller);
    if (!KSUCCESS(Status)) {
        goto GicInitializeIoUnitEnd;
    }

    //
    // Mask every interrupt in the distributor.
    //

    for (BlockIndex = 0;
         BlockIndex < Controller->MaxLines / 32;
         BlockIndex += 1) {

        WRITE_GIC_DISTRIBUTOR(Controller,
                              GicDistributorEnableClear + (4 * BlockIndex),
                              0xFFFFFFFF);
    }

    //
    // Enable all the software generated interrupts (lines 0-16).
    //

    WRITE_GIC_DISTRIBUTOR(Controller, GicDistributorEnableSet, 0x0000FFFF);

    //
    // Enable the GIC distributor.
    //

    WRITE_GIC_DISTRIBUTOR(Controller,
                          GicDistributorControl,
                          GIC_DISTRIBUTOR_CONTROL_ENABLE);

    Status = STATUS_SUCCESS;

GicInitializeIoUnitEnd:
    return Status;
}

KSTATUS
HlpGicSetLocalUnitAddressing (
    PVOID Context,
    PINTERRUPT_HARDWARE_TARGET Target
    )

/*++

Routine Description:

    This routine attempts to set the current processor's addressing mode.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Target - Supplies a pointer to the targeting configuration to set for this
        processor.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNSUCCESSFUL if the operation failed.

    STATUS_NOT_SUPPORTED if this configuration is never supported on this
        hardware.

--*/

{

    KSTATUS Status;
    ULONG ThisProcessorTarget;

    ThisProcessorTarget = ArGetMultiprocessorIdRegister() &
                          ARM_PROCESSOR_ID_MASK;

    switch (Target->Addressing) {
    case InterruptAddressingLogicalClustered:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case InterruptAddressingPhysical:
        if (Target->U.PhysicalId != ThisProcessorTarget) {
            Status = STATUS_UNSUCCESSFUL;
            goto GicSetLocalUnitAddressingEnd;
        }

        Status = STATUS_SUCCESS;
        break;

    case InterruptAddressingLogicalFlat:
        if (Target->U.LogicalFlatId !=
            (1 << (ThisProcessorTarget & GIC_PROCESSOR_ID_MASK))) {

            Status = STATUS_UNSUCCESSFUL;
            goto GicSetLocalUnitAddressingEnd;
        }

        Status = STATUS_SUCCESS;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto GicSetLocalUnitAddressingEnd;
    }

GicSetLocalUnitAddressingEnd:
    return Status;
}

INTERRUPT_CAUSE
HlpGicInterruptBegin (
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

    ULONG AcknowledgeRegister;
    PGIC_DISTRIBUTOR_DATA Controller;
    ULONG Line;

    Controller = (PGIC_DISTRIBUTOR_DATA)Context;

    //
    // Read the interrupt acknowledge register, which accepts the highest
    // priority interrupt (marking it from pending to active). Save this in the
    // magic candy area because the mask of which processors sent this interrupt
    // will need to be remembered when EOIing it.
    //

    AcknowledgeRegister =
                   READ_GIC_CPU_INTERFACE(GicCpuInterfaceInterruptAcknowledge);

    Line = AcknowledgeRegister & GIC_CPU_INTERFACE_ACKNOWLEDGE_LINE_MASK;
    if (Line == GIC_SPURIOUS_LINE) {
        return InterruptCauseSpuriousInterrupt;
    }

    *MagicCandy = AcknowledgeRegister;
    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->U.Local.Controller = Controller->Identifier;
    FiringLine->U.Local.Line = Line;
    return InterruptCauseLineFired;
}

VOID
HlpGicEndOfInterrupt (
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
    // Write the value put into the opaque token into the EOI register.
    //

    WRITE_GIC_CPU_INTERFACE(GicCpuInterfaceEndOfInterrupt, MagicCandy);
    return;
}

KSTATUS
HlpGicRequestInterrupt (
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

    ULONG CommandValue;
    PGIC_DISTRIBUTOR_DATA Controller;
    KSTATUS Status;

    Controller = (PGIC_DISTRIBUTOR_DATA)Context;

    //
    // Currently requesting device interrupts is not supported. This support
    // will probably have to be added when deep power management comes online.
    //

    CommandValue = Line->U.Local.Line;
    if (CommandValue >= GIC_SOFTWARE_INTERRUPT_LINE_COUNT) {
        Status = STATUS_NOT_IMPLEMENTED;
        goto GicRequestInterruptEnd;
    }

    Status = STATUS_SUCCESS;
    switch (Target->Addressing) {
    case InterruptAddressingLogicalClustered:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case InterruptAddressingSelf:
        CommandValue |= GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_SELF_SHORTHAND;
        break;

    case InterruptAddressingAll:
        CommandValue |= 0xFF << GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_TARGET_SHIFT;
        break;

    case InterruptAddressingAllExcludingSelf:
        CommandValue |=
                     GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_ALL_BUT_SELF_SHORTHAND;

        break;

    case InterruptAddressingLogicalFlat:
        CommandValue |= Target->U.LogicalFlatId <<
                        GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_TARGET_SHIFT;

        break;

    case InterruptAddressingPhysical:
        CommandValue |= (1 << (Target->U.PhysicalId & GIC_PROCESSOR_ID_MASK)) <<
                        GIC_DISTRIBUTOR_SOFTWARE_INTERRUPT_TARGET_SHIFT;

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto GicRequestInterruptEnd;
    }

    //
    // Write the command out to the software interrupt register.
    //

    WRITE_GIC_DISTRIBUTOR(Controller,
                          GicDistributorSoftwareInterrupt,
                          CommandValue);

GicRequestInterruptEnd:
    return Status;
}

KSTATUS
HlpGicStartProcessor (
    PVOID Context,
    ULONG Identifier,
    PHYSICAL_ADDRESS JumpAddressPhysical
    )

/*++

Routine Description:

    This routine attempts to start the given processor.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Identifier - Supplies the identifier of the processor to start.

    JumpAddressPhysical - Supplies the physical address of the location that
        new processor should jump to.

Return Value:

    STATUS_SUCCESS if the start command was successfully sent.

    Error code on failure.

--*/

{

    INTERRUPT_LINE Line;
    KSTATUS Status;
    INTERRUPT_HARDWARE_TARGET Target;

    Line.Type = InterruptLineControllerSpecified;
    Line.U.Local.Controller = 0;
    Line.U.Local.Line = 0;
    Target.Addressing = InterruptAddressingPhysical;
    Target.U.PhysicalId = Identifier;
    Status = HlpGicRequestInterrupt(Context, &Line, 0, &Target);
    return Status;
}

KSTATUS
HlpGicSetLineState (
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

    ULONG Configuration;
    ULONG ConfigurationBlock;
    ULONG ConfigurationShift;
    PGIC_DISTRIBUTOR_DATA Controller;
    ULONG LineBit;
    ULONG LineBlock;
    ULONG LineNumber;
    UCHAR PriorityValue;
    KSTATUS Status;
    UCHAR Target;
    UCHAR ThisProcessorTarget;

    Controller = (PGIC_DISTRIBUTOR_DATA)Context;
    LineNumber = Line->U.Local.Line;
    LineBlock = (LineNumber / 32) * 4;
    LineBit = LineNumber % 32;
    Status = STATUS_SUCCESS;

    //
    // Fail if the system is trying to set a really wacky interrupt line number.
    //

    if (LineNumber >= GIC_MAX_LINES) {
        Status = STATUS_INVALID_PARAMETER;
        goto GicSetLineStateEnd;
    }

    //
    // Simply clear out the line if it is being disabled.
    //

    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
        WRITE_GIC_DISTRIBUTOR(Controller,
                              GicDistributorEnableClear + LineBlock,
                              1 << LineBit);

        Status = STATUS_SUCCESS;
        goto GicSetLineStateEnd;
    }

    //
    // Set the priority of the requested line.
    //

    PriorityValue = SYSTEM_TO_GIC_PRIORITY(State->HardwarePriority);
    WRITE_GIC_DISTRIBUTOR_BYTE(Controller,
                               GicDistributorPriority + LineNumber,
                               PriorityValue);

    //
    // Set the targeting of the interrupt.
    //

    Target = 0;
    switch (State->Target.Addressing) {
    case InterruptAddressingLogicalClustered:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case InterruptAddressingSelf:
        Target = 1 << (ArGetMultiprocessorIdRegister() & GIC_PROCESSOR_ID_MASK);
        break;

    case InterruptAddressingAll:
        Target = 0xFF;
        break;

    case InterruptAddressingAllExcludingSelf:
        ThisProcessorTarget = 1 << (ArGetMultiprocessorIdRegister() &
                                    GIC_PROCESSOR_ID_MASK);

        Target = 0xFF & (~ThisProcessorTarget);
        break;

    case InterruptAddressingLogicalFlat:
        Target = State->Target.U.LogicalFlatId & GIC_PROCESSOR_ID_MASK;
        break;

    case InterruptAddressingPhysical:
        Target = 1 << (State->Target.U.PhysicalId & GIC_PROCESSOR_ID_MASK);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto GicSetLineStateEnd;
    }

    WRITE_GIC_DISTRIBUTOR_BYTE(Controller,
                               GicDistributorInterruptTarget + LineNumber,
                               Target);

    //
    // Set the configuration register.
    //

    ConfigurationBlock = 4 * (LineNumber / 16);
    ConfigurationShift = 2 * (LineNumber % 16);
    Configuration = READ_GIC_DISTRIBUTOR(
                    Controller,
                    GicDistributorInterruptConfiguration + ConfigurationBlock);

    //
    // Mask out all the bits being set, then set the appropriate ones.
    //

    Configuration &= ~(GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_MASK <<
                       ConfigurationShift);

    if (State->Mode == InterruptModeEdge) {
        Configuration |=
                      GIC_DISTRIBUTOR_INTERRUPT_CONFIGURATION_EDGE_TRIGGERED <<
                      ConfigurationShift;
    }

    WRITE_GIC_DISTRIBUTOR(
                     Controller,
                     GicDistributorInterruptConfiguration + ConfigurationBlock,
                     Configuration);

    //
    // Enable the line.
    //

    WRITE_GIC_DISTRIBUTOR(Controller,
                          GicDistributorEnableSet + LineBlock,
                          1 << LineBit);

GicSetLineStateEnd:
    return Status;
}

VOID
HlpGicMaskLine (
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

    PGIC_DISTRIBUTOR_DATA Controller;
    ULONG LineBit;
    ULONG LineBlock;
    GIC_DISTRIBUTOR_REGISTER Register;

    Controller = (PGIC_DISTRIBUTOR_DATA)Context;
    LineBlock = (Line->U.Local.Line / 32) * 4;
    LineBit = Line->U.Local.Line % 32;
    Register = GicDistributorEnableClear;
    if (Enable != FALSE) {
        Register = GicDistributorEnableSet;
    }

    WRITE_GIC_DISTRIBUTOR(Controller, Register + LineBlock, 1 << LineBit);
    return;
}

KSTATUS
HlpGicResetLocalUnit (
    VOID
    )

/*++

Routine Description:

    This routine resets the current processor's GIC CPU Interface.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    //
    // Set the binary point register to define where the priority group ends
    // and the subgroup begins. Initialize it to the most conservative value
    // that all implementations must support.
    //

    WRITE_GIC_CPU_INTERFACE(GicCpuInterfaceBinaryPoint,
                            GIC_CPU_INTERFACE_BINARY_POINT_MINIMUM);

    //
    // Set the running priority to its lowest value.
    //

    WRITE_GIC_CPU_INTERFACE(GicCpuInterfacePriorityMask,
                            SYSTEM_TO_GIC_PRIORITY(0));

    //
    // Enable this CPU interface.
    //

    WRITE_GIC_CPU_INTERFACE(GicCpuInterfaceControl,
                            GIC_CPU_INTERFACE_CONTROL_ENABLE);

    return STATUS_SUCCESS;
}

KSTATUS
HlpGicDescribeLines (
    PGIC_DISTRIBUTOR_DATA Controller
    )

/*++

Routine Description:

    This routine describes all lines to the system.

Arguments:

    Controller - Supplies a pointer to the interrupt controller.

Return Value:

    Status code.

--*/

{

    INTERRUPT_LINES_DESCRIPTION Lines;
    KSTATUS Status;

    RtlZeroMemory(&Lines, sizeof(INTERRUPT_LINES_DESCRIPTION));
    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;

    //
    // Describe the SGIs.
    //

    Lines.Type = InterruptLinesSoftwareOnly;
    Lines.Controller = Controller->Identifier;
    Lines.LineStart = 0;
    Lines.LineEnd = GIC_SOFTWARE_INTERRUPT_LINE_COUNT;
    Lines.Gsi = Controller->GsiBase;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto GicDescribeLinesEnd;
    }

    //
    // Register the PPIs.
    //

    Lines.Type = InterruptLinesProcessorLocal;
    Lines.LineStart = GIC_PROCESSOR_PERIPHERAL_LINE_BASE;
    Lines.LineEnd = Lines.LineStart + GIC_PROCESSOR_PERIPHERAL_LINE_COUNT;
    Lines.Gsi += GIC_SOFTWARE_INTERRUPT_LINE_COUNT;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto GicDescribeLinesEnd;
    }

    //
    // Register the normal lines.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.LineStart = GIC_PROCESSOR_NORMAL_LINE_BASE;
    Lines.LineEnd = Lines.LineStart +
                    Controller->MaxLines -
                    GIC_SOFTWARE_INTERRUPT_LINE_COUNT -
                    GIC_PROCESSOR_PERIPHERAL_LINE_COUNT;

    Lines.Gsi += GIC_PROCESSOR_PERIPHERAL_LINE_COUNT;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto GicDescribeLinesEnd;
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
        goto GicDescribeLinesEnd;
    }

GicDescribeLinesEnd:
    return Status;
}

KSTATUS
HlpGicSetupIoUnitAccess (
    PGIC_DISTRIBUTOR_DATA Controller
    )

/*++

Routine Description:

    This routine ensures that the GIC distributor is mapped and available.

Arguments:

    Controller - Supplies a pointer to the controller context.

Return Value:

    Status code.

--*/

{

    ULONG LineCountField;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    if (Controller->Distributor == NULL) {
        PhysicalAddress = Controller->PhysicalAddress;
        Controller->Distributor = HlMapPhysicalAddress(PhysicalAddress,
                                                       GIC_DISTRIBUTOR_SIZE,
                                                       TRUE);

        if (Controller->Distributor == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GicSetupIoUnitAccessEnd;
        }

        //
        // Determine the maximum number of lines that this controller may have.
        //

        LineCountField = READ_GIC_DISTRIBUTOR(Controller, GicDistributorType) &
                         GIC_DISTRIBUTOR_TYPE_LINE_COUNT_MASK;

        Controller->MaxLines = 32 * (LineCountField + 1);
        Status = HlpGicDescribeLines(Controller);
        if (!KSUCCESS(Status)) {
            goto GicSetupIoUnitAccessEnd;
        }
    }

    Status = STATUS_SUCCESS;

GicSetupIoUnitAccessEnd:
    return Status;
}


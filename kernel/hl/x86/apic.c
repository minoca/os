/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    apic.c

Abstract:

    This module implements support for the Advanced Programmable Interrupt
    Controller.

Author:

    Evan Green 5-Aug-2012

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
// Define the APIC allocation tag.
//

#define APIC_ALLOCATION_TAG 0x43495041 // 'CIPA'

//
// Define the size of the APIC register blocks.
//

#define LOCAL_APIC_REGISTER_SIZE 0x1000
#define IO_APIC_REGISTER_SIZE 0x1000

//
// Define the artificial IPI line number.
//

#define APIC_IPI_LINE 0x10

//
// Define the artificial offset where physical lines begin.
//

#define IO_APIC_LINE_OFFSET 0x20

//
// Define the NMI vector to watch out for.
//

#define APIC_NMI_VECTOR 0x02

//
// Define the bits for APIC MSI/MSI-X addresses.
//

#define APIC_MSI_ADDRESS_LOCAL_APIC_MASK      0xFFF00000
#define APIC_MSI_ADDRESS_DESTINATION_ID_MASK  0x000FF000
#define APIC_MSI_ADDRESS_DESTINATION_ID_SHIFT 12
#define APIC_MSI_ADDRESS_REDIRECTION_ENABLED  0x00000008
#define APIC_MSI_ADDRESS_LOGICAL_MODE         0x00000004

//
// Define the bits for APIC MSI/MSI-X data.
//

#define APIC_MSI_DATA_LEVEL_TRIGGERED 0x00008000
#define APIC_MSI_DATA_EDGE_TRIGGERED  0x00000000
#define APIC_MSI_DATA_LEVEL_ASSERT    0x00004000
#define APIC_MSI_DATA_LEVEL_DEASSERT  0x00000000
#define APIC_MSI_DATA_DELIVER_FIXED   0x00000000
#define APIC_MSI_DATA_DELIVER_LOWEST  0x00000100
#define APIC_MSI_DATA_DELIVER_SMI     0x00000200
#define APIC_MSI_DATA_DELIVER_NMI     0x00000400
#define APIC_MSI_DATA_DELIVER_INIT    0x00000500
#define APIC_MSI_DATA_DELIVER_EXT_INT 0x00000700
#define APIC_MSI_DATA_VECTOR_MASK     0x000000FF
#define APIC_MSI_DATA_VECTOR_SHIFT    0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes data internal to the APIC hardware module about
    an I/O APIC.

Members:

    PhysicalAddress - Stores the physical address of the I/O APIC's base.

    IoApic - Stores the virtual address of the mapping to the I/O APIC.

    GsiBase - Stores the global system interrupt base of the I/O APIC.

    Identifier - Stores the identifier of this I/O APIC.

--*/

typedef struct _IO_APIC_DATA {
    PVOID IoApic;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG GsiBase;
    ULONG Identifier;
    ULONG LineCount;
} IO_APIC_DATA, *PIO_APIC_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpApicEnumerateProcessors (
    PVOID Context,
    PPROCESSOR_DESCRIPTION Descriptions,
    ULONG DescriptionsBufferSize
    );

KSTATUS
HlpApicInitializeLocalUnit (
    PVOID Context,
    PULONG Identifier
    );

KSTATUS
HlpApicInitializeIoUnit (
    PVOID Context
    );

KSTATUS
HlpApicSetLocalUnitAddressing (
    PVOID Context,
    PINTERRUPT_HARDWARE_TARGET Target
    );

VOID
HlpApicFastEndOfInterrupt (
    VOID
    );

KSTATUS
HlpApicRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

KSTATUS
HlpApicStartProcessor (
    PVOID Context,
    ULONG Identifier,
    PHYSICAL_ADDRESS JumpAddressPhysical
    );

KSTATUS
HlpApicSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResouceDataSize
    );

VOID
HlpApicMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

KSTATUS
HlpApicGetMessageInformation (
    ULONGLONG Vector,
    ULONGLONG VectorCount,
    PINTERRUPT_HARDWARE_TARGET Target,
    PINTERRUPT_LINE OutputLine,
    ULONG Flags,
    PMSI_INFORMATION Information
    );

KSTATUS
HlpApicResetLocalUnit (
    VOID
    );

KSTATUS
HlpApicDescribeLines (
    PIO_APIC_DATA Controller
    );

KSTATUS
HlpApicConvertToRte (
    PINTERRUPT_LINE_STATE State,
    PULONG RteHigh,
    PULONG RteLow
    );

ULONG
HlpIoApicReadRegister (
    PIO_APIC_DATA IoApic,
    IO_APIC_REGISTER Register
    );

VOID
HlpIoApicWriteRegister (
    PIO_APIC_DATA IoApic,
    IO_APIC_REGISTER Register,
    ULONG Value
    );

ULONGLONG
HlpIoApicReadRedirectionTableEntry (
    PIO_APIC_DATA IoApic,
    ULONG EntryNumber
    );

VOID
HlpIoApicWriteRedirectionTableEntry (
    PIO_APIC_DATA IoApic,
    ULONG EntryNumber,
    ULONGLONG Entry
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Remember a pointer to the MADT.
//

PMADT HlApicMadt = NULL;

//
// Store a pointer to the local APIC. It is assumed that all local APICs are at
// the same physical address.
//

PVOID HlLocalApic = NULL;

//
// Store the identifier of the first I/O APIC.
//

ULONG HlFirstIoApicId;

//
// Store the interrupt function table template.
//

INTERRUPT_FUNCTION_TABLE HlApicInterruptFunctionTable = {
    HlpApicInitializeIoUnit,
    HlpApicSetLineState,
    HlpApicMaskLine,
    NULL,
    HlpApicFastEndOfInterrupt,
    NULL,
    HlpApicRequestInterrupt,
    HlpApicEnumerateProcessors,
    HlpApicInitializeLocalUnit,
    HlpApicSetLocalUnitAddressing,
    HlpApicStartProcessor,
    HlpApicGetMessageInformation
};

//
// Define the local APIC interrupts to mask off initially.
//

const UCHAR HlApicLvts[] = {
    ApicTimerVector,
    ApicLInt0Vector,
    ApicLInt1Vector,
    0
};

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpApicModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the APIC hardware module. Its role is to
    detect and report the prescense of an APIC.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PMADT_GENERIC_ENTRY CurrentEntry;
    PMADT_IO_APIC IoApic;
    PIO_APIC_DATA IoApicData;
    PMADT MadtTable;
    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    ULONG ProcessorCount;
    KSTATUS Status;

    //
    // Attempt to find an MADT. If one exists, then an APIC is present.
    //

    MadtTable = HlGetAcpiTable(MADT_SIGNATURE, NULL);
    if (MadtTable == NULL) {
        goto ApicModuleEntryEnd;
    }

    HlApicMadt = MadtTable;

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

        if ((CurrentEntry->Type == MadtEntryTypeLocalApic) &&
            (CurrentEntry->Length == sizeof(MADT_LOCAL_APIC))) {

            ProcessorCount += 1;
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

    //
    // Fail if the MADT is malformed and no processors are present.
    //

    if (ProcessorCount == 0) {
        goto ApicModuleEntryEnd;
    }

    //
    // Loop through again to register all IOAPICs. Associate all processors with
    // the first IOAPIC stumbled across.
    //

    CurrentEntry = (PMADT_GENERIC_ENTRY)(MadtTable + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)MadtTable + MadtTable->Header.Length)) {

        if ((CurrentEntry->Type == MadtEntryTypeIoApic) &&
            (CurrentEntry->Length == sizeof(MADT_IO_APIC))) {

            IoApic = (PMADT_IO_APIC)CurrentEntry;
            if (HlFirstIoApicId == 0) {
                HlFirstIoApicId = IoApic->IoApicId;
            }

            //
            // Allocate context needed for this I/O APIC.
            //

            IoApicData = HlAllocateMemory(sizeof(IO_APIC_DATA),
                                          APIC_ALLOCATION_TAG,
                                          FALSE,
                                          NULL);

            if (IoApicData == NULL) {
                goto ApicModuleEntryEnd;
            }

            RtlZeroMemory(IoApicData, sizeof(IO_APIC_DATA));
            IoApicData->PhysicalAddress = IoApic->IoApicAddress;
            IoApicData->IoApic = NULL;
            IoApicData->GsiBase = IoApic->GsiBase;
            IoApicData->Identifier = IoApic->IoApicId;

            //
            // Initialize the new controller structure.
            //

            NewController.TableVersion =
                                       INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;

            RtlCopyMemory(&(NewController.FunctionTable),
                          &HlApicInterruptFunctionTable,
                          sizeof(INTERRUPT_FUNCTION_TABLE));

            NewController.Context = IoApicData;
            NewController.Identifier = IoApic->IoApicId;
            NewController.ProcessorCount = ProcessorCount;
            NewController.PriorityCount = APIC_PRIORITY_COUNT;
            ProcessorCount = 0;

            //
            // Register the controller with the system.
            //

            Status = HlRegisterHardware(HardwareModuleInterruptController,
                                        &NewController);

            if (!KSUCCESS(Status)) {
                goto ApicModuleEntryEnd;
            }
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

ApicModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpApicEnumerateProcessors (
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
    PMADT_LOCAL_APIC LocalApic;
    PMADT MadtTable;
    ULONG ProcessorCount;
    KSTATUS Status;

    MadtTable = HlApicMadt;
    if (MadtTable == NULL) {
        Status = STATUS_NOT_INITIALIZED;
        goto ApicEnumerateProcessorsEnd;
    }

    //
    // Loop through every entry in the MADT looking for local APICs.
    //

    ProcessorCount = 0;
    CurrentProcessor = Descriptions;
    CurrentEntry = (PMADT_GENERIC_ENTRY)(MadtTable + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)MadtTable + MadtTable->Header.Length)) {

        if ((CurrentEntry->Type == MadtEntryTypeLocalApic) &&
            (CurrentEntry->Length == sizeof(MADT_LOCAL_APIC))) {

            LocalApic = (PMADT_LOCAL_APIC)CurrentEntry;
            ProcessorCount += 1;

            //
            // Fail if the buffer is not big enough for this processor.
            //

            if (sizeof(PROCESSOR_DESCRIPTION) * ProcessorCount >
                DescriptionsBufferSize) {

                Status = STATUS_BUFFER_TOO_SMALL;
                goto ApicEnumerateProcessorsEnd;
            }

            CurrentProcessor->Version = PROCESSOR_DESCRIPTION_VERSION;
            CurrentProcessor->PhysicalId = LocalApic->ApicId;
            CurrentProcessor->LogicalFlatId = 0;
            if (LocalApic->ApicId < 8) {
                CurrentProcessor->LogicalFlatId = 1 << LocalApic->ApicId;
            }

            CurrentProcessor->FirmwareIdentifier = LocalApic->AcpiProcessorId;
            CurrentProcessor->Flags = 0;
            if ((LocalApic->Flags & MADT_LOCAL_APIC_FLAG_ENABLED) != 0) {
                CurrentProcessor->Flags |= PROCESSOR_DESCRIPTION_FLAG_PRESENT;
            }

            CurrentProcessor += 1;
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

    Status = STATUS_SUCCESS;

ApicEnumerateProcessorsEnd:
    return Status;
}

KSTATUS
HlpApicInitializeLocalUnit (
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
    KSTATUS Status;

    //
    // Map the APIC to virtual address space if that has not been done so yet.
    //

    if (HlLocalApic == NULL) {
        if (HlApicMadt == NULL) {
            Status = STATUS_NOT_INITIALIZED;
            goto ApicInitializeLocalUnitEnd;
        }

        PhysicalAddress = HlApicMadt->ApicAddress;
        HlLocalApic = HlMapPhysicalAddress(PhysicalAddress,
                                           LOCAL_APIC_REGISTER_SIZE,
                                           TRUE);

        if (HlLocalApic == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ApicInitializeLocalUnitEnd;
        }
    }

    Status = HlpApicResetLocalUnit();
    if (!KSUCCESS(Status)) {
        goto ApicInitializeLocalUnitEnd;
    }

    *Identifier = READ_LOCAL_APIC(ApicId) >> APIC_DESTINATION_SHIFT;

ApicInitializeLocalUnitEnd:
    return Status;
}

KSTATUS
HlpApicInitializeIoUnit (
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

    PIO_APIC_DATA Controller;
    ULONGLONG Entry;
    ULONG LineIndex;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    ULONG VersionRegister;

    Controller = (PIO_APIC_DATA)Context;

    //
    // Map the controller if it has not yet been mapped. Also take this as a
    // cue to describe all lines to the system.
    //

    if (Controller->IoApic == NULL) {
        PhysicalAddress = Controller->PhysicalAddress;
        Controller->IoApic = HlMapPhysicalAddress(PhysicalAddress,
                                                  IO_APIC_REGISTER_SIZE,
                                                  TRUE);

        if (Controller->IoApic == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ApicInitializeIoUnitEnd;
        }

        //
        // Get the number of lines in the I/O APIC.
        //

        VersionRegister = HlpIoApicReadRegister(Controller,
                                                IoApicRegisterVersion);

        Controller->LineCount = (VersionRegister &
                                 IO_APIC_VERSION_MAX_ENTRY_MASK) >>
                                IO_APIC_VERSION_MAX_ENTRY_SHIFT;

        Controller->LineCount += 1;
        Status = HlpApicDescribeLines(Controller);
        if (!KSUCCESS(Status)) {
            goto ApicInitializeIoUnitEnd;
        }
    }

    //
    // Mask all interrupt lines.
    //

    Entry = IO_APIC_MASKED_RTE_VALUE;
    for (LineIndex = 0; LineIndex < Controller->LineCount; LineIndex += 1) {
        HlpIoApicWriteRedirectionTableEntry(Controller, LineIndex, Entry);
    }

    Status = STATUS_SUCCESS;

ApicInitializeIoUnitEnd:
    return Status;
}

KSTATUS
HlpApicSetLocalUnitAddressing (
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

    ULONG LogicalDestination;
    ULONG OriginalVector;
    KSTATUS Status;

    //
    // Intel says the destination format register must be set before the
    // APIC is software enabled.
    //

    OriginalVector = READ_LOCAL_APIC(ApicSpuriousVector);
    WRITE_LOCAL_APIC(ApicSpuriousVector, VECTOR_SPURIOUS_INTERRUPT);
    switch (Target->Addressing) {

    //
    // If targeted physically, zero out the logical destination register and
    // default to clustered.
    //

    case InterruptAddressingPhysical:
        WRITE_LOCAL_APIC(ApicDestinationFormat, APIC_LOGICAL_CLUSTERED);
        WRITE_LOCAL_APIC(ApicLogicalDestination, 0);
        break;

    case InterruptAddressingLogicalFlat:
        WRITE_LOCAL_APIC(ApicDestinationFormat, APIC_LOGICAL_FLAT);
        LogicalDestination = Target->U.LogicalFlatId << APIC_DESTINATION_SHIFT;
        WRITE_LOCAL_APIC(ApicLogicalDestination, LogicalDestination);
        if (READ_LOCAL_APIC(ApicLogicalDestination) != LogicalDestination) {
            Status = STATUS_NOT_SUPPORTED;
            goto SetProcessorTargetingEnd;
        }

        break;

    case InterruptAddressingLogicalClustered:
        WRITE_LOCAL_APIC(ApicDestinationFormat, APIC_LOGICAL_CLUSTERED);
        LogicalDestination = Target->U.Cluster.Id << APIC_MAX_CLUSTER_SIZE;
        LogicalDestination |= Target->U.Cluster.Mask;
        LogicalDestination <<= APIC_DESTINATION_SHIFT;
        WRITE_LOCAL_APIC(ApicLogicalDestination, LogicalDestination);
        if (READ_LOCAL_APIC(ApicLogicalDestination) != LogicalDestination) {
            Status = STATUS_NOT_SUPPORTED;
            goto SetProcessorTargetingEnd;
        }

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto SetProcessorTargetingEnd;
    }

    Status = STATUS_SUCCESS;

SetProcessorTargetingEnd:
    WRITE_LOCAL_APIC(ApicSpuriousVector, OriginalVector);
    return Status;
}

VOID
HlpApicFastEndOfInterrupt (
    VOID
    )

/*++

Routine Description:

    This routine sends the End Of Interrupt command to the APIC.

Arguments:

    None.

Return Value:

    None.

--*/

{

    WRITE_LOCAL_APIC(ApicEndOfInterrupt, 0);
    return;
}

KSTATUS
HlpApicRequestInterrupt (
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

    ULONG IpiHigh;
    ULONG IpiLow;
    ULONG IrrMask;
    ULONG IrrOffset;
    KSTATUS Status;
    BOOL TargetingSelf;

    IpiHigh = 0;
    IpiLow = Vector | APIC_EDGE_TRIGGERED;
    if (Vector == APIC_NMI_VECTOR) {
        IpiLow |= APIC_DELIVER_NMI;
    }

    //
    // Currently only IPIs are supported.
    //

    if (Line->U.Local.Line != APIC_IPI_LINE) {
        Status = STATUS_NOT_SUPPORTED;
        goto RequestInterruptEnd;
    }

    TargetingSelf = FALSE;
    switch (Target->Addressing) {
    case InterruptAddressingPhysical:
        IpiLow |= APIC_PHYSICAL_DELIVERY;
        IpiHigh = Target->U.PhysicalId << APIC_ID_SHIFT;
        if (IpiHigh == READ_LOCAL_APIC(ApicId)) {
            TargetingSelf = TRUE;
        }

        break;

    case InterruptAddressingLogicalFlat:
        IpiLow |= APIC_LOGICAL_DELIVERY;
        IpiHigh = Target->U.LogicalFlatId << APIC_ID_SHIFT;
        if ((READ_LOCAL_APIC(ApicLogicalDestination) & IpiHigh) == IpiHigh) {
            TargetingSelf = TRUE;
        }

        break;

    case InterruptAddressingLogicalClustered:
        IpiLow |= APIC_LOGICAL_DELIVERY;
        IpiHigh = (Target->U.Cluster.Id << (APIC_ID_SHIFT + 4)) |
                  (Target->U.Cluster.Mask << APIC_ID_SHIFT);

        if ((READ_LOCAL_APIC(ApicLogicalDestination) & IpiHigh) == IpiHigh) {
            TargetingSelf = TRUE;
        }

        break;

    case InterruptAddressingAll:
        TargetingSelf = TRUE;
        IpiLow |= APIC_SHORTHAND_ALL_INCLUDING_SELF;
        break;

    case InterruptAddressingAllExcludingSelf:
        IpiLow |= APIC_SHORTHAND_ALL_EXCLUDING_SELF;
        break;

    case InterruptAddressingSelf:
        TargetingSelf = TRUE;
        IpiLow |= APIC_SHORTHAND_SELF;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto RequestInterruptEnd;
    }

    //
    // Wait for any previously pending IPIs to clear.
    //

    while ((READ_LOCAL_APIC(ApicCommandLow) & APIC_DELIVERY_PENDING) != 0) {
        NOTHING;
    }

    //
    // Write the high value and then the low value. Writing the low value
    // actually sends the command.
    //

    WRITE_LOCAL_APIC(ApicCommandHigh, IpiHigh);
    WRITE_LOCAL_APIC(ApicCommandLow, IpiLow);

    //
    // If the interrupt is targeted at this processor, wait for the bit to be
    // set in the IRR. The IRR is laid out as a bitmask of vectors, 0 to 255.
    // There are 32 bits per register.
    //

    if (TargetingSelf != FALSE) {
        IrrOffset = ApicInterruptRequest + (Vector / 32);
        IrrMask = 1 << (Vector % 32);
        while ((READ_LOCAL_APIC(IrrOffset) & IrrMask) == 0) {
            NOTHING;
        }
    }

    Status = STATUS_SUCCESS;

RequestInterruptEnd:
    return Status;
}

KSTATUS
HlpApicStartProcessor (
    PVOID Context,
    ULONG Identifier,
    PHYSICAL_ADDRESS JumpAddressPhysical
    )

/*++

Routine Description:

    This routine sends a "start interrupt" (INIT-SIPI-SIPI) to the given
    processor.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Identifier - Supplies the identifier of the processor to start.

    JumpAddressPhysical - Supplies the physical address of the location that
        new processor should jump to.

    JumpAddressVirtual - Supplies the virtual address corresponding to the
        physical address that the processor should jump to.

Return Value:

    STATUS_SUCCESS if the start command was successfully sent.

    Error code on failure.

--*/

{

    ULONG InitIpi;
    ULONG StartupIpi;

    if (((UINTN)JumpAddressPhysical & ~APIC_STARTUP_CODE_MASK) != 0) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Wait for the command register to clear.
    //

    while ((READ_LOCAL_APIC(ApicCommandLow) & APIC_DELIVERY_PENDING) != 0) {
        NOTHING;
    }

    //
    // Write the physical identifier of the processor to start in the high
    // part of the IPI register.
    //

    WRITE_LOCAL_APIC(ApicCommandHigh, Identifier << APIC_DESTINATION_SHIFT);
    InitIpi = APIC_DELIVER_INIT | APIC_PHYSICAL_DELIVERY | APIC_LEVEL_ASSERT |
              APIC_EDGE_TRIGGERED;

    //
    // Send the INIT IPI to the requested processor, and wait for the
    // delivery status bit to clear.
    //

    WRITE_LOCAL_APIC(ApicCommandLow, InitIpi);
    while ((READ_LOCAL_APIC(ApicCommandLow) & APIC_DELIVERY_PENDING) != 0) {
        NOTHING;
    }

    //
    // Stall to let things settle.
    //

    HlBusySpin(10000);

    //
    // Send the INIT Deassert IPI to take the processor out of reset.
    //

    InitIpi = APIC_DELIVER_INIT | APIC_PHYSICAL_DELIVERY | APIC_LEVEL_DEASSERT |
              APIC_LEVEL_TRIGGERED;

    WRITE_LOCAL_APIC(ApicCommandLow, InitIpi);
    while ((READ_LOCAL_APIC(ApicCommandLow) & APIC_DELIVERY_PENDING) != 0) {
        NOTHING;
    }

    //
    // Send the SIPI (Startup IPIs) to the processor. The vector field contains
    // the address to jump to.
    //

    StartupIpi = ((ULONG)JumpAddressPhysical & APIC_STARTUP_CODE_MASK) >>
                 APIC_STARTUP_CODE_SHIFT;

    StartupIpi |= APIC_DELIVER_STARTUP | APIC_LEVEL_ASSERT |
                  APIC_EDGE_TRIGGERED | APIC_PHYSICAL_DELIVERY;

    WRITE_LOCAL_APIC(ApicCommandLow, StartupIpi);
    while ((READ_LOCAL_APIC(ApicCommandLow) & APIC_DELIVERY_PENDING) != 0) {
        NOTHING;
    }

    //
    // Send the second SIPI.
    //

    WRITE_LOCAL_APIC(ApicCommandLow, StartupIpi);
    return STATUS_SUCCESS;
}

KSTATUS
HlpApicSetLineState (
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

    PIO_APIC_DATA Controller;
    ULONGLONG Entry;
    ULONG LocalLine;
    LOCAL_APIC_REGISTER Register;
    ULONG RteHigh;
    ULONG RteLow;
    KSTATUS Status;

    Controller = (PIO_APIC_DATA)Context;
    LocalLine = Line->U.Local.Line;

    //
    // Convert the line state into interrupt controller format.
    //

    Status = HlpApicConvertToRte(State, &RteHigh, &RteLow);
    if (!KSUCCESS(Status)) {
        goto ApicSetLineStateEnd;
    }

    //
    // Handle LVTs.
    //

    if (LocalLine < ApicLineCount) {
        switch (LocalLine) {
        case ApicLineTimer:
            Register = ApicTimerVector;

            //
            // Change the RTE to avoid disturbing the existing bits in the LVT.
            //

            RteLow = (READ_LOCAL_APIC(Register) & ~0xFF) | State->Vector;
            break;

        case ApicLineThermal:
            Register = ApicThermalSensorVector;
            break;

        case ApicLinePerformance:
            Register = ApicPerformanceMonitorVector;
            break;

        case ApicLineLInt0:
            Register = ApicLInt0Vector;
            break;

        case ApicLineLInt1:
            Register = ApicLInt1Vector;
            break;

        case ApicLineError:
            Register = ApicErrorVector;
            break;

        case ApicLineCmci:
            Register = ApicLvtCmci;
            break;

        default:
            Status = STATUS_NOT_IMPLEMENTED;
            goto ApicSetLineStateEnd;
        }

        //
        // Program the configuration into the correct LVT.
        //

        WRITE_LOCAL_APIC(Register, RteLow);
        Status = STATUS_SUCCESS;

    //
    // Handle the IPI line.
    //

    } else if (LocalLine == APIC_IPI_LINE) {
        Status = STATUS_SUCCESS;
        goto ApicSetLineStateEnd;

    //
    // Handle actual I/O APIC entries.
    //

    } else {
        Entry = ((ULONGLONG)RteHigh << 32) | RteLow;
        HlpIoApicWriteRedirectionTableEntry(Controller,
                                            LocalLine - IO_APIC_LINE_OFFSET,
                                            Entry);

        Status = STATUS_SUCCESS;
    }

ApicSetLineStateEnd:
    return Status;
}

VOID
HlpApicMaskLine (
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

    ULONGLONG Entry;
    ULONG LocalLine;

    LocalLine = Line->U.Local.Line - IO_APIC_LINE_OFFSET;
    Entry = HlpIoApicReadRedirectionTableEntry(Context, LocalLine);
    Entry &= ~APIC_RTE_MASKED;
    if (Enable == FALSE) {
        Entry |= APIC_RTE_MASKED;
    }

    HlpIoApicWriteRedirectionTableEntry(Context, LocalLine, Entry);
    return;
}

KSTATUS
HlpApicGetMessageInformation (
    ULONGLONG Vector,
    ULONGLONG VectorCount,
    PINTERRUPT_HARDWARE_TARGET Target,
    PINTERRUPT_LINE OutputLine,
    ULONG Flags,
    PMSI_INFORMATION Information
    )

/*++

Routine Description:

    This routine gathers the appropriate MSI/MSI-X address and data information
    for the given set of contiguous interrupt vectors.

Arguments:

    Vector - Supplies the first vector for which information is being requested.

    VectorCount - Supplies the number of contiguous vectors for which
        information is being requested.

    Target - Supplies a pointer to the set of processors to target.

    OutputLine - Supplies the output line this interrupt line should interrupt
        to.

    Flags - Supplies a bitfield of flags about the operation. See
        INTERRUPT_LINE_STATE_FLAG_* definitions.

    Information - Supplies a pointer to an array of MSI/MSI-X information to
        be filled in by the routine.

Return Value:

    Status code.

--*/

{

    ULONGLONG Data;
    ULONGLONG DataVector;
    ULONGLONG Index;
    ULONG LocalApicId;
    ULONG LogicalAddress;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    if (HlApicMadt == NULL) {
        Status = STATUS_NOT_INITIALIZED;
        goto ApicGetMessageInformation;
    }

    //
    // Fill in the MSI/MSI-X information for each vector.
    //

    PhysicalAddress = HlApicMadt->ApicAddress &
                      APIC_MSI_ADDRESS_LOCAL_APIC_MASK;

    //
    // Determine the processor target routing.
    //

    switch (Target->Addressing) {
    case InterruptAddressingAll:
        PhysicalAddress |= (0xFF << APIC_MSI_ADDRESS_DESTINATION_ID_SHIFT);
        break;

    case InterruptAddressingPhysical:
        PhysicalAddress |= (Target->U.PhysicalId <<
                            APIC_MSI_ADDRESS_DESTINATION_ID_SHIFT) &
                           APIC_MSI_ADDRESS_DESTINATION_ID_MASK;

        break;

    case InterruptAddressingSelf:
        LocalApicId = READ_LOCAL_APIC(ApicId) >> APIC_DESTINATION_SHIFT;
        PhysicalAddress |= (LocalApicId <<
                            APIC_MSI_ADDRESS_DESTINATION_ID_SHIFT) &
                           APIC_MSI_ADDRESS_DESTINATION_ID_MASK;

        break;

    case InterruptAddressingLogicalClustered:
        LogicalAddress = (Target->U.Cluster.Id << APIC_CLUSTER_SHIFT) |
                         Target->U.Cluster.Mask;

        PhysicalAddress |= (LogicalAddress <<
                            APIC_MSI_ADDRESS_DESTINATION_ID_SHIFT) &
                           APIC_MSI_ADDRESS_DESTINATION_ID_MASK;

        PhysicalAddress |= APIC_MSI_ADDRESS_LOGICAL_MODE;
        PhysicalAddress |= APIC_MSI_ADDRESS_REDIRECTION_ENABLED;
        break;

    case InterruptAddressingLogicalFlat:
        LogicalAddress = Target->U.LogicalFlatId;
        PhysicalAddress |= (LogicalAddress <<
                            APIC_MSI_ADDRESS_DESTINATION_ID_SHIFT) &
                           APIC_MSI_ADDRESS_DESTINATION_ID_MASK;

        PhysicalAddress |= APIC_MSI_ADDRESS_LOGICAL_MODE;
        PhysicalAddress |= APIC_MSI_ADDRESS_REDIRECTION_ENABLED;
        break;

    case InterruptAddressingAllExcludingSelf:
    default:
        Status = STATUS_INVALID_PARAMETER;
        goto ApicGetMessageInformation;
    }

    //
    // Calculate the MSI/MSI-X data value. MSIs are always edge triggered.
    //

    Data = APIC_MSI_DATA_EDGE_TRIGGERED;
    switch (OutputLine->U.Local.Line) {
    case INTERRUPT_CPU_LINE_NORMAL_INTERRUPT:
        if ((Flags & INTERRUPT_LINE_STATE_FLAG_LOWEST_PRIORITY) != 0) {
            Data |= APIC_MSI_DATA_DELIVER_LOWEST;

        } else {
            Data |= APIC_MSI_DATA_DELIVER_FIXED;
        }

        break;

    case INTERRUPT_CPU_LINE_NMI:
        Data |= APIC_MSI_DATA_DELIVER_NMI;
        break;

    case INTERRUPT_CPU_LINE_SMI:
        Data |= APIC_MSI_DATA_DELIVER_SMI;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto ApicGetMessageInformation;
    }

    //
    // Initialize the information for each vector. Make sure to OR in the
    // vector into each data value.
    //

    for (Index = 0; Index < VectorCount; Index += 1) {
        Information[Index].Address = PhysicalAddress;
        DataVector = ((Vector + Index) & APIC_MSI_DATA_VECTOR_MASK) <<
                     APIC_MSI_DATA_VECTOR_SHIFT;

        Information[Index].Data = Data | DataVector;
    }

    Status = STATUS_SUCCESS;

ApicGetMessageInformation:
    return Status;
}

KSTATUS
HlpApicResetLocalUnit (
    VOID
    )

/*++

Routine Description:

    This routine resets the current processor's local APIC.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    ULONG Lvt;
    ULONG SpuriousRegister;
    KSTATUS Status;
    ULONG Version;

    //
    // Get the APIC version number, located in the bottom 8 bits of the version
    // register. The version should be 0x1X, where X is probably 4.
    //

    Version = READ_LOCAL_APIC(ApicVersion);
    if ((Version & 0xF0) != 0x10) {
        Status = STATUS_VERSION_MISMATCH;
        goto ApicResetLocalUnitEnd;
    }

    //
    // Turn on the APIC (bit 8 in the spurious vector register), and
    // simultaneously program the spurious vector number.
    //

    SpuriousRegister = READ_LOCAL_APIC(ApicSpuriousVector);
    SpuriousRegister &= ~APIC_SPURIOUS_VECTOR_MASK;
    SpuriousRegister |= APIC_ENABLE | VECTOR_SPURIOUS_INTERRUPT;
    WRITE_LOCAL_APIC(ApicSpuriousVector, SpuriousRegister);

    //
    // Disable LVT entries such as the timer, LINT0, and LINT1. Leave the
    // delivery routing alone.
    //

    Index = 0;
    while (HlApicLvts[Index] != 0) {
        Lvt = READ_LOCAL_APIC(HlApicLvts[Index]) & APIC_DELIVERY_MASK;
        Lvt |= APIC_LVT_DISABLED | (0x80 + Index);
        WRITE_LOCAL_APIC(HlApicLvts[Index], Lvt);
        Index += 1;
    }

    WRITE_LOCAL_APIC(ApicTimerInitialCount, 0);
    Status = STATUS_SUCCESS;

ApicResetLocalUnitEnd:
    return Status;
}

KSTATUS
HlpApicDescribeLines (
    PIO_APIC_DATA Controller
    )

/*++

Routine Description:

    This routine describes all lines to the system.

Arguments:

    Controller - Supplies a pointer to the I/O APIC to describe.

Return Value:

    Status code.

--*/

{

    INTERRUPT_LINES_DESCRIPTION Lines;
    KSTATUS Status;

    RtlZeroMemory(&Lines, sizeof(INTERRUPT_LINES_DESCRIPTION));
    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;
    Lines.Controller = Controller->Identifier;

    //
    // If this is the I/O APIC all the processors are attached to, describe the
    // local lines.
    //

    if (Controller->Identifier == HlFirstIoApicId) {

        //
        // Describe the LVTs.
        //

        Lines.Type = InterruptLinesProcessorLocal;
        Lines.LineStart = 0;
        Lines.LineEnd = ApicLineCount;
        Lines.Gsi = INTERRUPT_LINES_GSI_NONE;
        Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
        if (!KSUCCESS(Status)) {
            goto ApicDescribeLinesEnd;
        }

        //
        // Register the IPI line.
        //

        Lines.Type = InterruptLinesSoftwareOnly;
        Lines.LineStart = APIC_IPI_LINE;
        Lines.LineEnd = Lines.LineStart + 1;
        Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
        if (!KSUCCESS(Status)) {
            goto ApicDescribeLinesEnd;
        }
    }

    //
    // Register the output lines.
    //

    Lines.Type = InterruptLinesOutput;
    Lines.OutputControllerIdentifier = INTERRUPT_CPU_IDENTIFIER;
    Lines.LineStart = INTERRUPT_PC_MIN_CPU_LINE;
    Lines.LineEnd = INTERRUPT_PC_MAX_CPU_LINE;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto ApicDescribeLinesEnd;
    }

    //
    // Register the I/O APIC lines.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.LineStart = IO_APIC_LINE_OFFSET;
    Lines.LineEnd = Lines.LineStart + Controller->LineCount;
    Lines.Gsi = Controller->GsiBase;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto ApicDescribeLinesEnd;
    }

ApicDescribeLinesEnd:
    return Status;
}

KSTATUS
HlpApicConvertToRte (
    PINTERRUPT_LINE_STATE State,
    PULONG RteHigh,
    PULONG RteLow
    )

/*++

Routine Description:

    This routine converts the given line state into an APIC RTE, which can be
    used in the I/O APIC RTEs, APIC LVTs, and MSI messages.

Arguments:

    State - Supplies a pointer to the state to convert.

    RteHigh - Supplies a pointer where the high 32 bits of the Redirection
        Table Entry will be returned.

    RteLow - Supplies a pointer where the low 32 bits of the Redirection
        Table Entry will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the given configuration cannot be converted to
        an RTE.

--*/

{

    ULONG LogicalAddress;
    KSTATUS Status;

    *RteHigh = 0;
    *RteLow = State->Vector;

    //
    // Disabled lines are easy.
    //

    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
        *RteLow = IO_APIC_MASKED_RTE_VALUE;
        return STATUS_SUCCESS;
    }

    //
    // Determine the delivery mode based on the output line.
    //

    if ((State->Output.Type != InterruptLineControllerSpecified) ||
        (State->Output.U.Local.Controller != INTERRUPT_CPU_IDENTIFIER)) {

        Status = STATUS_INVALID_PARAMETER;
        goto ConvertToRteEnd;
    }

    switch (State->Output.U.Local.Line) {
    case INTERRUPT_CPU_LINE_NORMAL_INTERRUPT:
        if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_LOWEST_PRIORITY) != 0) {
            *RteLow |= APIC_DELIVER_LOWEST;

        } else {
            *RteLow |= APIC_DELIVER_FIXED;
        }

        break;

    case INTERRUPT_CPU_LINE_NMI:
        *RteLow |= APIC_DELIVER_NMI;
        break;

    case INTERRUPT_CPU_LINE_SMI:
        *RteLow |= APIC_DELIVER_SMI;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto ConvertToRteEnd;
    }

    //
    // Determine the processor target routing.
    //

    switch (State->Target.Addressing) {
    case InterruptAddressingAll:
        *RteHigh = 0xFF << APIC_DESTINATION_SHIFT;
        break;

    case InterruptAddressingPhysical:
        *RteHigh = State->Target.U.PhysicalId << APIC_DESTINATION_SHIFT;
        break;

    case InterruptAddressingSelf:
        *RteHigh = READ_LOCAL_APIC(ApicId);
        break;

    case InterruptAddressingLogicalClustered:
        LogicalAddress = (State->Target.U.Cluster.Id << APIC_CLUSTER_SHIFT) |
                         (State->Target.U.Cluster.Mask);

        *RteHigh = LogicalAddress << APIC_DESTINATION_SHIFT;
        *RteLow |= APIC_LOGICAL_DELIVERY;
        break;

    case InterruptAddressingLogicalFlat:
        LogicalAddress = State->Target.U.LogicalFlatId;
        *RteHigh = LogicalAddress << APIC_DESTINATION_SHIFT;
        *RteLow |= APIC_LOGICAL_DELIVERY;
        break;

    //
    // None of the shorthands can ever be used on an I/O APIC RTE, but they
    // can be used for IPI command registers.
    //

    case InterruptAddressingAllExcludingSelf:
        *RteLow |= APIC_SHORTHAND_ALL_EXCLUDING_SELF;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto ConvertToRteEnd;
    }

    //
    // Determine the trigger mode and polarity.
    //

    if (State->Mode == InterruptModeLevel) {
        *RteLow |= APIC_LEVEL_TRIGGERED;
    }

    if (State->Polarity == InterruptActiveLow) {
        *RteLow |= APIC_ACTIVE_LOW;
    }

    Status = STATUS_SUCCESS;

ConvertToRteEnd:
    return Status;
}

ULONG
HlpIoApicReadRegister (
    PIO_APIC_DATA IoApic,
    IO_APIC_REGISTER Register
    )

/*++

Routine Description:

    This routine reads from the given I/O APIC register.

Arguments:

    IoApic - Supplies a pointer to the I/O APIC to read from.

    Register - Supplies the register to read.

Return Value:

    Returns the contents of the requested I/O APIC register.

--*/

{

    //
    // Write the register number to the window.
    //

    HlWriteRegister32(IoApic->IoApic + IO_APIC_SELECT_OFFSET, Register);

    //
    // Return the contents of the window at that register.
    //

    return HlReadRegister32(IoApic->IoApic + IO_APIC_DATA_OFFSET);
}

VOID
HlpIoApicWriteRegister (
    PIO_APIC_DATA IoApic,
    IO_APIC_REGISTER Register,
    ULONG Value
    )

/*++

Routine Description:

    This routine writes to the given I/O APIC register.

Arguments:

    IoApic - Supplies a pointer to the I/O APIC to write to.

    Register - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    //
    // Write the register number to the window.
    //

    HlWriteRegister32(IoApic->IoApic + IO_APIC_SELECT_OFFSET, Register);

    //
    // Write the value into the register.
    //

    HlWriteRegister32(IoApic->IoApic + IO_APIC_DATA_OFFSET, Value);
    return;
}

ULONGLONG
HlpIoApicReadRedirectionTableEntry (
    PIO_APIC_DATA IoApic,
    ULONG EntryNumber
    )

/*++

Routine Description:

    This routine returns a I/O APIC Redirection Table Entry (RTE).

Arguments:

    IoApic - Supplies a pointer to the I/O APIC to read from.

    EntryNumber - Supplies the RTE entry to read.

Return Value:

    Returns the requested Redirection Table Entry.

--*/

{

    ULONGLONG Entry;
    ULONGLONG EntryHigh;
    ULONG Offset;

    Offset = IoApicRegisterFirstRedirectionTableEntry +
             (EntryNumber * IO_APIC_RTE_SIZE);

    Entry = HlpIoApicReadRegister(IoApic, Offset);
    EntryHigh = HlpIoApicReadRegister(IoApic, Offset + 1);
    Entry |= EntryHigh << 32;
    return Entry;
}

VOID
HlpIoApicWriteRedirectionTableEntry (
    PIO_APIC_DATA IoApic,
    ULONG EntryNumber,
    ULONGLONG Entry
    )

/*++

Routine Description:

    This routine writes an I/O APIC Redirection Table Entry (RTE).

Arguments:

    IoApic - Supplies a pointer to the I/O APIC to write to.

    EntryNumber - Supplies the RTE entry to write.

    Entry - Supplies the redirection table entry to write.

Return Value:

    None.

--*/

{

    ULONG Offset;

    Offset = IoApicRegisterFirstRedirectionTableEntry +
             (EntryNumber * IO_APIC_RTE_SIZE);

    //
    // Mask the low part, then write the high part, then the real low part.
    //

    HlpIoApicWriteRegister(IoApic, Offset, IO_APIC_MASKED_RTE_VALUE);
    HlpIoApicWriteRegister(IoApic, Offset + 1, (ULONG)(Entry >> 32));
    HlpIoApicWriteRegister(IoApic, Offset, (ULONG)Entry);
    return;
}


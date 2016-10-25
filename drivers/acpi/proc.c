/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    proc.c

Abstract:

    This module implements support for processor devices in ACPI.

Author:

    Evan Green 28-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpip.h"
#include "proc.h"
#include "namespce.h"
#include "resdesc.h"

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
AcpipCreateGlobalProcessorContext (
    VOID
    );

VOID
AcpipProcessorInitializeCStates (
    PACPI_OBJECT NamespaceObject,
    PACPI_PROCESSOR_CONTEXT Device
    );

KSTATUS
AcpipInitializeCStatesOnProcessor (
    PPM_IDLE_STATE_INTERFACE Interface,
    PPM_IDLE_PROCESSOR_STATE Processor
    );

KSTATUS
AcpipProcessorGetOsProcessorId (
    ULONG AcpiId,
    PULONG OsId
    );

ULONG
AcpipGetProcessorCount (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this boolean to print processor power enumeration information.
//

BOOL AcpiDebugProcessorPowerEnumeration = FALSE;

//
// Remember the number of processors and how many have successfully started.
//

ULONG AcpiProcessorCount;
ULONG AcpiInitializedProcessorCount;

PACPI_PROCESSOR_GLOBAL_CONTEXT AcpiProcessor;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipProcessorStart (
    PACPI_DEVICE_CONTEXT Device
    )

/*++

Routine Description:

    This routine starts an ACPI processor object.

Arguments:

    Device - Supplies a pointer to the ACPI information associated with
        the processor device.

Return Value:

    Status code.

--*/

{

    ULONG AcpiId;
    ULONG BlockAddress;
    ULONG BlockSize;
    PACPI_OBJECT NamespaceObject;
    ULONG OsId;
    PACPI_PROCESSOR_CONTEXT Processor;
    ULONG ReadyCount;
    UINTN Size;
    KSTATUS Status;
    PACPI_OBJECT UidMethod;
    PACPI_OBJECT UidReturnValue;

    Processor = NULL;
    UidReturnValue = NULL;
    if (Device->Processor != NULL) {
        Status = STATUS_SUCCESS;
        goto ProcessorStartEnd;
    }

    if (AcpiProcessor == NULL) {
        Status = AcpipCreateGlobalProcessorContext();
        if (!KSUCCESS(Status)) {
            goto ProcessorStartEnd;
        }
    }

    //
    // Perform architecture specific initialization before evaluating processor
    // methods, such as calling _OSC.
    //

    Status = AcpipArchInitializeProcessorManagement(Device->NamespaceObject);
    if (!KSUCCESS(Status)) {
        goto ProcessorStartEnd;
    }

    //
    // Get the ACPI processor ID.
    //

    AcpiId = -1;
    BlockAddress = 0;
    BlockSize = 0;
    NamespaceObject = Device->NamespaceObject;
    if (NamespaceObject->Type == AcpiObjectProcessor) {
        AcpiId = NamespaceObject->U.Processor.ProcessorId;
        BlockAddress = NamespaceObject->U.Processor.ProcessorBlockAddress;
        BlockSize = NamespaceObject->U.Processor.ProcessorBlockLength;

    } else {

        ASSERT(NamespaceObject->Type == AcpiObjectDevice);

        //
        // Attempt to find and execute the _UID function.
        //

        UidMethod = AcpipFindNamedObject(NamespaceObject, ACPI_METHOD__UID);
        if (UidMethod == NULL) {
            Status = STATUS_DEVICE_NOT_CONNECTED;
            goto ProcessorStartEnd;
        }

        Status = AcpiExecuteMethod(UidMethod, NULL, 0, 0, &UidReturnValue);
        if (!KSUCCESS(Status)) {
            goto ProcessorStartEnd;
        }

        if (UidReturnValue == NULL) {
            Status = STATUS_INVALID_CONFIGURATION;
            goto ProcessorStartEnd;
        }

        //
        // Convert to a device ID string if needed.
        //

        if (UidReturnValue->Type == AcpiObjectInteger) {
            AcpiId = UidReturnValue->U.Integer.Value;

        } else if (UidReturnValue->Type == AcpiObjectString) {

            //
            // Consider supporting processor strings if needed.
            //

            ASSERT(FALSE);

            Status = STATUS_NOT_SUPPORTED;
            goto ProcessorStartEnd;
        }
    }

    //
    // Match the ACPI processor ID with an entry in the MADT, and determine the
    // OS processor number from that.
    //

    Status = AcpipProcessorGetOsProcessorId(AcpiId, &OsId);
    if (!KSUCCESS(Status)) {
        goto ProcessorStartEnd;
    }

    ASSERT(OsId < AcpiProcessor->ProcessorCount);

    Processor = &(AcpiProcessor->Processors[OsId]);
    Processor->BlockAddress = BlockAddress;
    Processor->BlockSize = BlockSize;
    Processor->AcpiId = AcpiId;
    Processor->OsId = OsId;
    AcpipProcessorInitializeCStates(NamespaceObject, Processor);

    //
    // This processor device is initialized. If this was the last one, then
    // register processor power management facilities.
    //

    ReadyCount = RtlAtomicAdd32(&(AcpiProcessor->StartedProcessorCount), 1) + 1;

    ASSERT(ReadyCount <= AcpiProcessor->ProcessorCount);

    if (ReadyCount == AcpiProcessor->ProcessorCount) {

        //
        // Register C-State handlers. Failure is not fatal.
        //

        if (Processor->CStateCount != 0) {
            Size = sizeof(PM_IDLE_STATE_INTERFACE);
            KeGetSetSystemInformation(SystemInformationPm,
                                      PmInformationIdleStateHandlers,
                                      &(AcpiProcessor->CStateInterface),
                                      &Size,
                                      TRUE);
        }
    }

    Status = STATUS_SUCCESS;

ProcessorStartEnd:
    if (UidReturnValue != NULL) {
        AcpipObjectReleaseReference(UidReturnValue);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
AcpipCreateGlobalProcessorContext (
    VOID
    )

/*++

Routine Description:

    This routine creates and initializes the global ACPI processor context.

Arguments:

    None.

Return Value:

    None. Failure to initialize C-States is not fatal.

--*/

{

    UINTN AllocationSize;
    PACPI_PROCESSOR_GLOBAL_CONTEXT Context;
    ULONG ProcessorCount;
    KSTATUS Status;

    ProcessorCount = AcpipGetProcessorCount();
    AllocationSize = sizeof(ACPI_PROCESSOR_GLOBAL_CONTEXT) +
                     (sizeof(ACPI_PROCESSOR_CONTEXT) * ProcessorCount);

    Context = MmAllocateNonPagedPool(AllocationSize, ACPI_ALLOCATION_TAG);
    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateGlobalProcessorContextEnd;
    }

    RtlZeroMemory(Context, AllocationSize);
    Context->ProcessorCount = ProcessorCount;
    Context->Processors = (PACPI_PROCESSOR_CONTEXT)(Context + 1);
    Context->CStateInterface.InitializeIdleStates =
                                             AcpipInitializeCStatesOnProcessor;

    Context->CStateInterface.EnterIdleState = AcpipEnterCState;
    Context->CStateInterface.Context = Context;
    Status = STATUS_SUCCESS;

CreateGlobalProcessorContextEnd:
    if (!KSUCCESS(Status)) {
        if (Context != NULL) {
            MmFreeNonPagedPool(Context);
        }
    }

    //
    // If device enumeration is parallelized, this should be an atomic compare
    // exchange and then a subsequent free if the exchange is lost.
    //

    ASSERT(AcpiProcessor == NULL);

    AcpiProcessor = Context;
    return Status;
}

VOID
AcpipProcessorInitializeCStates (
    PACPI_OBJECT NamespaceObject,
    PACPI_PROCESSOR_CONTEXT Device
    )

/*++

Routine Description:

    This routine attempts to find and initialize the OS logical processor
    number for the given processor device.

Arguments:

    NamespaceObject - Supplies the processor namespace object.

    Device - Supplies a pointer to the ACPI information associated with
        the processor device. On success, the OS ID will be filled in.

Return Value:

    None. Failure to initialize C-States is not fatal.

--*/

{

    PACPI_CSTATE AcpiCState;
    ULONG Count;
    PACPI_OBJECT CountObject;
    PACPI_OBJECT Cst;
    ULONG CStateIndex;
    PACPI_OBJECT CStateLatencyObject;
    PACPI_OBJECT CStatePowerObject;
    PACPI_OBJECT CStateRegister;
    ACPI_CSTATE_TYPE CStateType;
    PACPI_OBJECT CStateTypeObject;
    PACPI_OBJECT CstMethod;
    ULONG Latency;
    PPM_IDLE_STATE OsCState;
    ULONG Power;
    PACPI_OBJECT StatePackage;
    KSTATUS Status;

    Cst = NULL;
    if (AcpiDebugProcessorPowerEnumeration != FALSE) {
        RtlDebugPrint("Processor %d (ACPI %d) C-States\n",
                      Device->OsId,
                      Device->AcpiId);
    }

    //
    // Attempt to find and execute the _CST function.
    //

    CstMethod = AcpipFindNamedObject(NamespaceObject, ACPI_METHOD__CST);
    if (CstMethod == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto ProcessorInitializeCStatesEnd;
    }

    Status = AcpiExecuteMethod(CstMethod, NULL, 0, 0, &Cst);
    if (!KSUCCESS(Status)) {
        goto ProcessorInitializeCStatesEnd;
    }

    if ((Cst == NULL) || (Cst->Type != AcpiObjectPackage) ||
        (Cst->U.Package.ElementCount < 2)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto ProcessorInitializeCStatesEnd;
    }

    CountObject = Cst->U.Package.Array[0];
    Count = CountObject->U.Integer.Value;
    if ((CountObject->Type != AcpiObjectInteger) ||
        (Count + 1 > Cst->U.Package.ElementCount)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto ProcessorInitializeCStatesEnd;
    }

    if (Count > ACPI_MAX_CSTATES) {
        RtlDebugPrint("%d C-States!\n", Count);
        Count = ACPI_MAX_CSTATES;
    }

    Device->HighestNonC3 = 0;

    //
    // Loop over all the enumerated C-states.
    //

    for (CStateIndex = 0; CStateIndex < Count; CStateIndex += 1) {
        StatePackage = Cst->U.Package.Array[1 + CStateIndex];
        if ((StatePackage->Type != AcpiObjectPackage) ||
            (StatePackage->U.Package.ElementCount < 4)) {

            Status = STATUS_INVALID_CONFIGURATION;
            goto ProcessorInitializeCStatesEnd;
        }

        CStateRegister = StatePackage->U.Package.Array[0];
        CStateTypeObject = StatePackage->U.Package.Array[1];
        CStateLatencyObject = StatePackage->U.Package.Array[2];
        CStatePowerObject = StatePackage->U.Package.Array[3];
        if ((CStateRegister->Type != AcpiObjectBuffer) ||
            (CStateTypeObject->Type != AcpiObjectInteger) ||
            (CStateLatencyObject->Type != AcpiObjectInteger) ||
            (CStatePowerObject->Type != AcpiObjectInteger)) {

            Status = STATUS_INVALID_CONFIGURATION;
            goto ProcessorInitializeCStatesEnd;
        }

        CStateType = CStateTypeObject->U.Integer.Value;
        Latency = CStateLatencyObject->U.Integer.Value;
        Power = CStatePowerObject->U.Integer.Value;
        AcpiCState = &(Device->AcpiCStates[CStateIndex]);
        OsCState = &(Device->OsCStates[CStateIndex]);
        Status = AcpipParseGenericAddress(CStateRegister,
                                          &(AcpiCState->Register));

        if (!KSUCCESS(Status)) {
            goto ProcessorInitializeCStatesEnd;
        }

        //
        // Determine if this is Intel-specific fixed function hardware.
        //

        if ((AcpiCState->Register.AddressSpaceId ==
             AddressSpaceFixedHardware) &&
            (AcpiCState->Register.RegisterBitWidth ==
             ACPI_FIXED_HARDWARE_INTEL)) {

            switch (AcpiCState->Register.RegisterBitOffset) {
            case ACPI_FIXED_HARDWARE_INTEL_CST_HALT:
                AcpiCState->Flags |= ACPI_CSTATE_HALT;
                break;

            case ACPI_FIXED_HARDWARE_INTEL_CST_IO_HALT:
                AcpiCState->Flags |= ACPI_CSTATE_IO_HALT;
                break;

            case ACPI_FIXED_HARDWARE_INTEL_CST_MWAIT:
                AcpiCState->Flags |= ACPI_CSTATE_MWAIT;
                if ((AcpiCState->Register.AccessSize &
                     ACPI_INTEL_MWAIT_BUS_MASTER_AVOIDANCE) != 0) {

                    AcpiCState->Flags |= ACPI_CSTATE_BUS_MASTER_AVOIDANCE;
                }

                break;
            }
        }

        AcpiCState->Type = CStateType;
        AcpiCState->Latency = Latency;
        AcpiCState->Power = Power;
        if (AcpiDebugProcessorPowerEnumeration != FALSE) {
            RtlDebugPrint("C%d: Type %d Latency %d us, Power %d mw\n",
                          CStateIndex + 1,
                          CStateType,
                          Latency,
                          Power);
        }

        //
        // Remember the highest C-state that is not C3 in case there is a
        // fallback from a C3 transition that couldn't happen.
        //

        if ((CStateType < AcpiC3) && (CStateIndex > Device->HighestNonC3)) {
            Device->HighestNonC3 = CStateIndex;
        }

        //
        // Initialize the target residency to twice the latency, assuming it
        // takes that much time to get in and get out.
        //

        OsCState->Name[0] = 'C';
        OsCState->Name[1] = '0' + CStateIndex + 1;
        OsCState->Name[2] = '\0';
        OsCState->ExitLatency = KeConvertMicrosecondsToTimeTicks(Latency);
        OsCState->TargetResidency = OsCState->ExitLatency * 2;
    }

    Device->CStateCount = Count;
    Status = STATUS_SUCCESS;

ProcessorInitializeCStatesEnd:
    if (!KSUCCESS(Status)) {
        if (AcpiDebugProcessorPowerEnumeration != FALSE) {
            RtlDebugPrint("ACPI: C-State init failed on P%d: %d\n",
                          Device->AcpiId,
                          Status);
        }
    }

    if (Cst != NULL) {
        AcpipObjectReleaseReference(Cst);
    }

    return;
}

KSTATUS
AcpipInitializeCStatesOnProcessor (
    PPM_IDLE_STATE_INTERFACE Interface,
    PPM_IDLE_PROCESSOR_STATE Processor
    )

/*++

Routine Description:

    This routine is called on a particular processor to initialize processor
    C-State support.

Arguments:

    Interface - Supplies a pointer to the interface.

    Processor - Supplies a pointer to the context for this processor.

Return Value:

    Status code.

--*/

{

    PACPI_PROCESSOR_CONTEXT Context;

    Context = &(AcpiProcessor->Processors[Processor->ProcessorNumber]);
    Processor->Context = Context;
    Processor->States = Context->OsCStates;
    Processor->StateCount = Context->CStateCount;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipProcessorGetOsProcessorId (
    ULONG AcpiId,
    PULONG OsId
    )

/*++

Routine Description:

    This routine attempts to find and initialize the OS logical processor
    number for the given processor device.

Arguments:

    AcpiId - Supplies the ACPI processor ID to look up.

    OsId - Supplies a pointer where the OS processor index will be returned on
        success.

Return Value:

    Status code.

--*/

{

    BOOL Active;
    PMADT_GENERIC_ENTRY CurrentEntry;
    BOOL Found;
    PMADT_GIC Gic;
    PMADT_LOCAL_APIC LocalApic;
    PMADT MadtTable;
    ULONG PhysicalId;
    KSTATUS Status;

    MadtTable = AcpiFindTable(MADT_SIGNATURE, NULL);
    if (MadtTable == NULL) {
        return STATUS_NOT_FOUND;
    }

    //
    // Find an MADT entry that matches this processor ID.
    //

    Found = FALSE;
    PhysicalId = -1;
    CurrentEntry = (PMADT_GENERIC_ENTRY)(MadtTable + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)MadtTable + MadtTable->Header.Length)) {

        if ((CurrentEntry->Type == MadtEntryTypeLocalApic) &&
            (CurrentEntry->Length == sizeof(MADT_LOCAL_APIC))) {

            LocalApic = (PMADT_LOCAL_APIC)CurrentEntry;
            if ((LocalApic->Flags & MADT_LOCAL_APIC_FLAG_ENABLED) != 0) {
                if (LocalApic->AcpiProcessorId == AcpiId) {
                    PhysicalId = LocalApic->ApicId;
                    Found = TRUE;
                    break;
                }
            }

        } else if ((CurrentEntry->Type == MadtEntryTypeGic) &&
                   (CurrentEntry->Length == sizeof(MADT_GIC))) {

            Gic = (PMADT_GIC)CurrentEntry;
            if ((Gic->Flags & MADT_LOCAL_GIC_FLAG_ENABLED) != 0) {
                if (Gic->AcpiProcessorId == AcpiId) {
                    PhysicalId = Gic->GicId;
                    Found = TRUE;
                    break;
                }
            }
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

    if (Found == FALSE) {
        return STATUS_NOT_FOUND;
    }

    Status = HlGetProcessorIndexFromId(PhysicalId, OsId, &Active);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    if (Active == FALSE) {
        return STATUS_NOT_STARTED;
    }

    return STATUS_SUCCESS;
}

ULONG
AcpipGetProcessorCount (
    VOID
    )

/*++

Routine Description:

    This routine determines the number of processors in the system.

Arguments:

    None.

Return Value:

    Returns the maximum number of processors in the system.

--*/

{

    PMADT_GENERIC_ENTRY CurrentEntry;
    PMADT_GIC Gic;
    PMADT_LOCAL_APIC LocalApic;
    PMADT MadtTable;
    ULONG ProcessorCount;

    MadtTable = AcpiFindTable(MADT_SIGNATURE, NULL);
    if (MadtTable == NULL) {
        return 1;
    }

    ProcessorCount = 0;
    CurrentEntry = (PMADT_GENERIC_ENTRY)(MadtTable + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)MadtTable + MadtTable->Header.Length)) {

        if ((CurrentEntry->Type == MadtEntryTypeLocalApic) &&
            (CurrentEntry->Length == sizeof(MADT_LOCAL_APIC))) {

            LocalApic = (PMADT_LOCAL_APIC)CurrentEntry;
            if ((LocalApic->Flags & MADT_LOCAL_APIC_FLAG_ENABLED) != 0) {
                ProcessorCount += 1;
            }

        } else if ((CurrentEntry->Type == MadtEntryTypeGic) &&
                   (CurrentEntry->Length == sizeof(MADT_GIC))) {

            Gic = (PMADT_GIC)CurrentEntry;
            if ((Gic->Flags & MADT_LOCAL_GIC_FLAG_ENABLED) != 0) {
                ProcessorCount += 1;
            }
        }

        CurrentEntry = (PMADT_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                             CurrentEntry->Length);
    }

    return ProcessorCount;
}


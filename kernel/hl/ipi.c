/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ipi.c

Abstract:

    This module implements support for Inter-Processor Interrupts (IPIs).

Author:

    Evan Green 27-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this flag if the processor is present and can be started.
//

#define PROCESSOR_ADDRESSING_FLAG_PRESENT 0x00000001

//
// Set this flag if the processor is running.
//

#define PROCESSOR_ADDRESSING_FLAG_STARTED 0x00000002

//
// Define the amount of time to wait for a processor to come online before
// declaring the system toast.
//

#define PROCESSOR_START_GRACE_PERIOD_SECONDS 5

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpInterruptSetupIpiLines (
    ULONG ProcessorNumber
    );

KSTATUS
HlpInterruptFindIpiLine (
    PINTERRUPT_CONTROLLER Controller,
    PINTERRUPT_LINE Line
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define an override that limits the system to one processor.
//

BOOL HlRunSingleProcessor = FALSE;

//
// Store the maximum number of processors in the system.
//

ULONG HlMaxProcessors = 0;

//
// Store an array defining the addressing mode of each processor, indexed by
// processor number.
//

PPROCESSOR_ADDRESSING HlProcessorTargets = NULL;

//
// Store the array of interrupts for each IPI type.
//

PKINTERRUPT HlIpiKInterrupt[MAX_IPI_LINE_COUNT];

//
// Store the maximum number of processors in the system that can be targeted
// in logical flat mode.
//

ULONG HlLogicalFlatLimit = 8;

//
// Store the number of processors per cluster and the maximum number of
// clusters allowed.
//

ULONG HlMaxClusterSize = 4;
ULONG HlMaxClusters = 0xF;

//
// Store a boolean indicating if any processors have been programmed in logical
// clustered mode.
//

BOOL HlLogicalClusteredMode = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
HlGetProcessorIndexFromId (
    ULONGLONG PhysicalId,
    PULONG ProcessorIndex,
    PBOOL Active
    )

/*++

Routine Description:

    This routine attempts to find the logical processor index of the processor
    with the given physical identifier.

Arguments:

    PhysicalId - Supplies the processor physical identifier.

    ProcessorIndex - Supplies a pointer where the processor index will be
        returned on success.

    Active - Supplies a pointer where a boolean will be returned indicating if
        this processor is present and active within the system.

Return Value:

    Status code.

--*/

{

    ULONG Count;
    ULONG Index;
    ULONG Mask;

    Count = HlMaxProcessors;
    if (HlProcessorTargets == NULL) {
        return STATUS_NOT_FOUND;
    }

    Mask = PROCESSOR_ADDRESSING_FLAG_PRESENT |
           PROCESSOR_ADDRESSING_FLAG_STARTED;

    for (Index = 0; Index < Count; Index += 1) {
        if (HlProcessorTargets[Index].PhysicalId == PhysicalId) {
            *ProcessorIndex = Index;
            *Active = FALSE;
            if ((HlProcessorTargets[Index].Flags & Mask) == Mask) {
                *Active = TRUE;
            }

            return STATUS_SUCCESS;
        }
    }

    return STATUS_NOT_FOUND;
}

KSTATUS
HlSendIpi (
    IPI_TYPE IpiType,
    PPROCESSOR_SET Processors
    )

/*++

Routine Description:

    This routine sends an Inter-Processor Interrupt (IPI) to the given set of
    processors.

Arguments:

    IpiType - Supplies the type of IPI to deliver.

    Processors - Supplies the set of processors to deliver the IPI to.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    BOOL Enabled;
    PINTERRUPT_LINE IpiLine;
    ULONG IpiLineIndex;
    ULONG Processor;
    KSTATUS Status;
    INTERRUPT_HARDWARE_TARGET Target;
    PINTERRUPT_HARDWARE_TARGET TargetPointer;
    ULONG Vector;

    ASSERT((KeGetRunLevel() >= RunLevelDispatch) ||
           (ArAreInterruptsEnabled() == FALSE));

    Processor = KeGetCurrentProcessorNumber();
    Controller = HlProcessorTargets[Processor].Controller;
    TargetPointer = &Target;

    //
    // Compute the interrupt target in terms the hardware can understand.
    //

    switch (Processors->Target) {
    case ProcessorTargetNone:
        Status = STATUS_SUCCESS;
        goto SendIpiEnd;

    case ProcessorTargetAll:
        Target.Addressing = InterruptAddressingAll;
        break;

    case ProcessorTargetAllExcludingSelf:
        Target.Addressing = InterruptAddressingAllExcludingSelf;
        break;

    case ProcessorTargetSelf:
        Target.Addressing = InterruptAddressingSelf;
        break;

    case ProcessorTargetSingleProcessor:
        TargetPointer = &(HlProcessorTargets[Processors->U.Number].Target);
        break;

    case ProcessorTargetAny:
    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto SendIpiEnd;
    }

    Vector = HlpInterruptGetIpiVector(IpiType);
    IpiLineIndex = HlpInterruptGetIpiLineIndex(IpiType);
    IpiLine = &(HlProcessorTargets[Processor].IpiLine[IpiLineIndex]);
    Enabled = ArDisableInterrupts();
    Status = Controller->FunctionTable.RequestInterrupt(
                                                    Controller->PrivateContext,
                                                    IpiLine,
                                                    Vector,
                                                    TargetPointer);

    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    if (!KSUCCESS(Status)) {
        goto SendIpiEnd;
    }

SendIpiEnd:
    return Status;
}

ULONG
HlGetMaximumProcessorCount (
    VOID
    )

/*++

Routine Description:

    This routine returns the maximum number of logical processors that this
    machine supports.

Arguments:

    None.

Return Value:

    Returns the maximum number of logical processors that may exist in the
    system.

--*/

{

    return HlMaxProcessors;
}

KSTATUS
HlStartAllProcessors (
    PPROCESSOR_START_ROUTINE StartRoutine,
    PULONG ProcessorsStarted
    )

/*++

Routine Description:

    This routine is called on the BSP, and starts all APs.

Arguments:

    StartRoutine - Supplies the routine the processors should jump to.

    ProcessorsStarted - Supplies a pointer where the number of processors
        started will be returned (the total number of processors in the system,
        including the boot processor).

Return Value:

    Status code.

--*/

{

    PVOID Context;
    PINTERRUPT_CONTROLLER Controller;
    BOOL Enabled;
    ULONGLONG GiveUpTime;
    ULONG Identifier;
    PHYSICAL_ADDRESS PhysicalJumpAddress;
    ULONG Processor;
    ULONG ProcessorCount;
    ULONG ProcessorsLaunched;
    PPROCESSOR_START_BLOCK StartBlock;
    KSTATUS Status;

    Enabled = FALSE;
    ProcessorsLaunched = 1;
    StartBlock = NULL;

    //
    // Fire up the identity stub, which is used not only to initialize other
    // processors but also to come out during resume.
    //

    Status = HlpInterruptPrepareIdentityStub();
    if (!KSUCCESS(Status)) {
        goto StartAllProcessorsEnd;
    }

    //
    // Set up P0's startup page, needed for resume.
    //

    Status = HlpInterruptPrepareForProcessorStart(0, NULL, NULL, NULL);
    if (!KSUCCESS(Status)) {
        goto StartAllProcessorsEnd;
    }

    //
    // Don't start any other cores if the debug flag is set.
    //

    if (HlRunSingleProcessor != FALSE) {
        Status = STATUS_SUCCESS;
        goto StartAllProcessorsEnd;
    }

    //
    // Bail now if this machine is not multiprocessor capable.
    //

    ProcessorCount = HlGetMaximumProcessorCount();
    if (ProcessorCount == 1) {
        Status = STATUS_SUCCESS;
        goto StartAllProcessorsEnd;
    }

    //
    // Loop through each processor and start it.
    //

    for (Processor = 1; Processor < ProcessorCount; Processor += 1) {

        //
        // Skip this processor if it's not present.
        //

        if ((HlProcessorTargets[Processor].Flags &
             PROCESSOR_ADDRESSING_FLAG_PRESENT) == 0) {

            continue;
        }

        Controller = HlpInterruptGetProcessorController(Processor);
        Context = Controller->PrivateContext;
        Identifier = HlProcessorTargets[Processor].PhysicalId;

        //
        // Prepare the kernel for the new processor coming online.
        //

        StartBlock = KePrepareForProcessorLaunch();
        if (StartBlock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto StartAllProcessorsEnd;
        }

        //
        // Perform any architecture specific steps needed to start this
        // proessor.
        //

        Enabled = ArDisableInterrupts();
        Status = HlpInterruptPrepareForProcessorStart(Processor,
                                                      StartBlock,
                                                      StartRoutine,
                                                      &PhysicalJumpAddress);

        if (!KSUCCESS(Status)) {
            goto StartAllProcessorsEnd;
        }

        //
        // Send the command to start the processor.
        //

        Status = Controller->FunctionTable.StartProcessor(Context,
                                                          Identifier,
                                                          PhysicalJumpAddress);

        if (Enabled != FALSE) {
            ArEnableInterrupts();
            Enabled = FALSE;
        }

        if (!KSUCCESS(Status)) {
            goto StartAllProcessorsEnd;
        }

        //
        // Figure out when to give up if the processor doesn't come online.
        //

        GiveUpTime = HlQueryTimeCounter() +
                     (HlQueryTimeCounterFrequency() *
                      PROCESSOR_START_GRACE_PERIOD_SECONDS);

        //
        // Wait for the processor to start up.
        //

        while (StartBlock->Started == FALSE) {
            ArProcessorYield();
            if (HlQueryTimeCounter() >= GiveUpTime) {
                break;
            }
        }

        if (StartBlock->Started == FALSE) {
            KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                          HL_CRASH_PROCESSOR_WONT_START,
                          Processor,
                          (UINTN)Controller,
                          (UINTN)&(HlProcessorTargets[Processor]));
        }

        ProcessorsLaunched += 1;
    }

    Status = STATUS_SUCCESS;

StartAllProcessorsEnd:
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    if (!KSUCCESS(Status)) {
        if (StartBlock != NULL) {
            KeFreeProcessorStartBlock(StartBlock, TRUE);
            StartBlock = NULL;
        }
    }

    *ProcessorsStarted = ProcessorsLaunched;
    return Status;
}

KSTATUS
HlpInitializeIpis (
    VOID
    )

/*++

Routine Description:

    This routine initialize IPI support in the system. It is called once on
    boot.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PINTERRUPT_CONTROLLER Controller;
    ULONG ControllerCount;
    ULONG ControllerIndex;
    PPROCESSOR_DESCRIPTION Descriptions;
    ULONG GlobalProcessorIndex;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    ULONG MaxProcessors;
    ULONG MaxProcessorsPerUnit;
    ULONG NextProcessorIndex;
    ULONG Offset;
    ULONG PageSize;
    PHYSICAL_ADDRESS ParkedAddress;
    PVOID ParkedAddressMapping;
    PHYSICAL_ADDRESS ParkedAddressPage;
    ULONG ProcessorIndex;
    KSTATUS Status;

    Descriptions = NULL;
    PageSize = MmPageSize();

    //
    // Loop through all controllers once to figure out how many processors the
    // largest interrupt controller owns, and the total number of processors.
    //

    MaxProcessorsPerUnit = 0;
    MaxProcessors = 0;
    ControllerCount = HlInterruptControllerCount;
    for (ControllerIndex = 0;
         ControllerIndex < ControllerCount;
         ControllerIndex += 1) {

        Controller = HlInterruptControllers[ControllerIndex];
        if (Controller == NULL) {
            continue;
        }

        MaxProcessors += Controller->ProcessorCount;
        if (Controller->ProcessorCount > MaxProcessorsPerUnit) {
            MaxProcessorsPerUnit = Controller->ProcessorCount;
        }
    }

    if (MaxProcessors == 0) {
        MaxProcessors = 1;
        MaxProcessorsPerUnit = 1;
    }

    //
    // Allocate the total processor array, and the temporary processor
    // description structure.
    //

    AllocationSize = MaxProcessors * sizeof(PROCESSOR_ADDRESSING);
    HlProcessorTargets = MmAllocateNonPagedPool(AllocationSize, HL_POOL_TAG);
    if (HlProcessorTargets == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeIpisEnd;
    }

    RtlZeroMemory(HlProcessorTargets, AllocationSize);
    AllocationSize = MaxProcessorsPerUnit * sizeof(PROCESSOR_DESCRIPTION);
    Descriptions = MmAllocateNonPagedPool(AllocationSize, HL_POOL_TAG);
    if (Descriptions == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeIpisEnd;
    }

    RtlZeroMemory(Descriptions, AllocationSize);

    //
    // Loop through the controllers again and grab the processor enumeration
    // info from them.
    //

    ControllerCount = HlInterruptControllerCount;
    NextProcessorIndex = 0;
    for (ControllerIndex = 0;
         ControllerIndex < ControllerCount;
         ControllerIndex += 1) {

        Controller = HlInterruptControllers[ControllerIndex];
        if ((Controller == NULL) || (Controller->ProcessorCount == 0)) {
            continue;
        }

        ASSERT(Controller->ProcessorCount <= MaxProcessorsPerUnit);

        AllocationSize = Controller->ProcessorCount *
                         sizeof(PROCESSOR_DESCRIPTION);

        Status = Controller->FunctionTable.EnumerateProcessors(
                                                    Controller->PrivateContext,
                                                    Descriptions,
                                                    AllocationSize);

        if (!KSUCCESS(Status)) {
            goto InitializeIpisEnd;
        }

        ASSERT(NextProcessorIndex + Controller->ProcessorCount <=
                                                                MaxProcessors);

        //
        // Loop through each processor in the returned array and create its
        // corresponding IPI target.
        //

        for (ProcessorIndex = 0;
             ProcessorIndex < Controller->ProcessorCount;
             ProcessorIndex += 1) {

            GlobalProcessorIndex = ProcessorIndex + NextProcessorIndex;
            if (Descriptions[ProcessorIndex].Version <
                PROCESSOR_DESCRIPTION_VERSION) {

                Status = STATUS_VERSION_MISMATCH;
                goto InitializeIpisEnd;
            }

            HlProcessorTargets[GlobalProcessorIndex].PhysicalId =
                                       Descriptions[ProcessorIndex].PhysicalId;

            HlProcessorTargets[GlobalProcessorIndex].LogicalFlatId =
                                    Descriptions[ProcessorIndex].LogicalFlatId;

            //
            // If any processor reports a logical flat ID of 0, then logical
            // flat mode is not supported.
            //

            if (HlProcessorTargets[GlobalProcessorIndex].LogicalFlatId == 0) {
                HlLogicalFlatLimit = 0;
            }

            if ((Descriptions[ProcessorIndex].Flags &
                 PROCESSOR_DESCRIPTION_FLAG_PRESENT) != 0) {

                HlProcessorTargets[GlobalProcessorIndex].Flags |=
                                             PROCESSOR_ADDRESSING_FLAG_PRESENT;
            }

            ParkedAddress = Descriptions[ProcessorIndex].ParkedPhysicalAddress;
            HlProcessorTargets[GlobalProcessorIndex].ParkedPhysicalAddress =
                                                                 ParkedAddress;

            HlProcessorTargets[GlobalProcessorIndex].Controller = Controller;

            //
            // If non-null, map the parked physical address to a VA.
            //

            if (ParkedAddress != INVALID_PHYSICAL_ADDRESS) {
                ParkedAddressPage = ALIGN_RANGE_DOWN(ParkedAddress, PageSize);
                Offset = ParkedAddress - ParkedAddressPage;
                ParkedAddressMapping = MmMapPhysicalAddress(ParkedAddressPage,
                                                            PageSize,
                                                            TRUE,
                                                            FALSE,
                                                            TRUE);

                HlProcessorTargets[GlobalProcessorIndex].ParkedVirtualAddress =
                                                 ParkedAddressMapping + Offset;

                if (ParkedAddressMapping == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto InitializeIpisEnd;
                }
            }
        }

        //
        // Up the global index.
        //

        NextProcessorIndex += Controller->ProcessorCount;
    }

    //
    // Make up a page for P0 on resume if there was none. The I/O buffer
    // structure is leaked since the page is permanent.
    //

    if (HlProcessorTargets[0].ParkedVirtualAddress == NULL) {
        IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
                        IO_BUFFER_FLAG_MAP_NON_CACHED |
                        IO_BUFFER_FLAG_KERNEL_MODE_DATA |
                        IO_BUFFER_FLAG_MEMORY_LOCKED;

        IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                              MAX_ULONG,
                                              PageSize,
                                              PageSize,
                                              IoBufferFlags);

        if (IoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeIpisEnd;
        }

        HlProcessorTargets[0].Target.Addressing = InterruptAddressingPhysical;
        HlProcessorTargets[0].ParkedPhysicalAddress =
                                         IoBuffer->Fragment[0].PhysicalAddress;

        HlProcessorTargets[0].ParkedVirtualAddress =
                                          IoBuffer->Fragment[0].VirtualAddress;
    }

    Status = STATUS_SUCCESS;

InitializeIpisEnd:
    if (KSUCCESS(Status)) {
        HlMaxProcessors = MaxProcessors;

    } else {
        if (HlProcessorTargets != NULL) {
            MmFreeNonPagedPool(HlProcessorTargets);
        }
    }

    if (Descriptions != NULL) {
        MmFreeNonPagedPool(Descriptions);
    }

    return Status;
}

KSTATUS
HlpSetupProcessorAddressing (
    ULONG Identifier
    )

/*++

Routine Description:

    This routine prepares the system to receive IPIs on the current processor.

Arguments:

    Identifier - Supplies the physical identifier of the processor's local unit.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG PhysicalId;
    PVOID PrivateContext;
    ULONG ProcessorIndex;
    ULONG SearchIndex;
    PINTERRUPT_SET_LOCAL_UNIT_ADDRESSING SetAddressing;
    KSTATUS Status;
    PROCESSOR_ADDRESSING SwapSpace;
    PINTERRUPT_HARDWARE_TARGET Targeting;

    //
    // Return immediately if there's only one processor in the system
    // (or in the special case of P0 the first go-round, that IPIs have not
    // been set up).
    //

    if (HlProcessorTargets == NULL) {
        return STATUS_SUCCESS;
    }

    //
    // Make sure that this processor is in the correct index.
    //

    ProcessorIndex = KeGetCurrentProcessorNumber();
    if (HlProcessorTargets[ProcessorIndex].PhysicalId != Identifier) {

        //
        // Go find this processor.
        //

        for (SearchIndex = 0; SearchIndex < HlMaxProcessors; SearchIndex += 1) {
            if (HlProcessorTargets[SearchIndex].PhysicalId == Identifier) {

                //
                // Crash if either of these processors are already started.
                //

                if (((HlProcessorTargets[ProcessorIndex].Flags &
                      PROCESSOR_ADDRESSING_FLAG_STARTED) != 0) ||
                    ((HlProcessorTargets[SearchIndex].Flags &
                      PROCESSOR_ADDRESSING_FLAG_STARTED) != 0)) {

                    KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                                  HL_CRASH_PROCESSOR_INDEXING_ERROR,
                                  ProcessorIndex,
                                  HlProcessorTargets[ProcessorIndex].PhysicalId,
                                  Identifier);

                }

                //
                // Swap the two processors.
                //

                RtlCopyMemory(&SwapSpace,
                              &(HlProcessorTargets[SearchIndex]),
                              sizeof(PROCESSOR_ADDRESSING));

                RtlCopyMemory(&(HlProcessorTargets[SearchIndex]),
                              &(HlProcessorTargets[ProcessorIndex]),
                              sizeof(PROCESSOR_ADDRESSING));

                RtlCopyMemory(&(HlProcessorTargets[ProcessorIndex]),
                              &SwapSpace,
                              sizeof(PROCESSOR_ADDRESSING));

                break;
            }
        }

        //
        // Crash if the processor wasn't found.
        //

        if (SearchIndex == HlMaxProcessors) {
            KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                          HL_CRASH_PROCESSOR_INDEXING_ERROR,
                          0xFFFFFFFF,
                          ProcessorIndex,
                          Identifier);
        }
    }

    Controller = HlProcessorTargets[ProcessorIndex].Controller;
    SetAddressing = Controller->FunctionTable.SetLocalUnitAddressing;
    PhysicalId = HlProcessorTargets[ProcessorIndex].PhysicalId;
    PrivateContext = Controller->PrivateContext;
    Targeting = &(HlProcessorTargets[ProcessorIndex].Target);

    //
    // If this processor has already been started, then setup the local unit
    // addressing (which must succeed) and finish.
    //

    if ((HlProcessorTargets[ProcessorIndex].Flags &
        PROCESSOR_ADDRESSING_FLAG_STARTED) != 0) {

        Status = SetAddressing(PrivateContext, Targeting);
        goto SetupProcessorAddressingEnd;
    }

    //
    // Attempt to program the system in logical flat mode if the number of
    // processors is below the limit.
    //

    Status = STATUS_NOT_SUPPORTED;
    if ((HlLogicalFlatLimit != 0) &&
        (HlMaxProcessors <= HlLogicalFlatLimit) &&
        (HlLogicalClusteredMode == FALSE)) {

        ASSERT(HlLogicalFlatLimit <= 32);

        Targeting->Addressing = InterruptAddressingLogicalFlat;
        Targeting->U.LogicalFlatId =
                              HlProcessorTargets[ProcessorIndex].LogicalFlatId;

        Status = SetAddressing(PrivateContext, Targeting);
    }

    //
    // If logical flat mode was a no-go, try for logical clustered mode.
    //

    if ((!KSUCCESS(Status)) &&
        (ProcessorIndex < HlMaxClusters * HlMaxClusterSize)) {

        Targeting->Addressing = InterruptAddressingLogicalClustered;
        Targeting->U.Cluster.Id = PhysicalId / HlMaxClusterSize;
        Targeting->U.Cluster.Mask = 1 << (PhysicalId % HlMaxClusterSize);
        Status = SetAddressing(PrivateContext, Targeting);

        //
        // If this worked, remember that a processor somewhere is programmed
        // to logical clustered mode (mixing of logical flat and logical
        // clustered is illegal).
        //

        if (KSUCCESS(Status)) {
            HlLogicalClusteredMode = TRUE;
        }
    }

    //
    // If logical clustered mode was a no-go, target physically. This must
    // succeed.
    //

    if (!KSUCCESS(Status)) {
        Targeting->Addressing = InterruptAddressingPhysical;
        Targeting->U.PhysicalId = PhysicalId;
        Status = SetAddressing(PrivateContext, Targeting);
        if (!KSUCCESS(Status)) {
            KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                          HL_CRASH_SET_PROCESSOR_ADDRESSING_FAILURE,
                          ProcessorIndex,
                          Targeting->U.PhysicalId,
                          Status);
        }
    }

    //
    // Set up the IPI lines on this processor.
    //

    Status = HlpInterruptSetupIpiLines(ProcessorIndex);
    if (!KSUCCESS(Status)) {
        goto SetupProcessorAddressingEnd;
    }

    //
    // Mark this processor as started, as it can now be IPIed.
    //

    HlProcessorTargets[ProcessorIndex].Flags |=
                                             PROCESSOR_ADDRESSING_FLAG_STARTED;

SetupProcessorAddressingEnd:
    return Status;
}

PINTERRUPT_CONTROLLER
HlpInterruptGetCurrentProcessorController (
    VOID
    )

/*++

Routine Description:

    This routine returns the interrupt controller that owns the current
    processor.

Arguments:

    None.

Return Value:

    Returns a pointer to the interrupt controller responsible for this
    processor.

    NULL on a non-multiprocessor capable machine.

--*/

{

    return HlpInterruptGetProcessorController(KeGetCurrentProcessorNumber());
}

PINTERRUPT_CONTROLLER
HlpInterruptGetProcessorController (
    ULONG ProcessorIndex
    )

/*++

Routine Description:

    This routine returns the interrupt controller that owns the given processor.

Arguments:

    ProcessorIndex - Supplies the zero-based index of the processor whose
        interrupt controller is desired.

Return Value:

    Returns a pointer to the interrupt controller responsible for the given
    processor.

    NULL on a non-multiprocessor capable machine.

--*/

{

    //
    // Bail if this is a uniprocessor machine.
    //

    if (HlProcessorTargets == NULL) {
        return NULL;
    }

    return HlProcessorTargets[ProcessorIndex].Controller;
}

KSTATUS
HlpInterruptConvertProcessorSetToInterruptTarget (
    PPROCESSOR_SET ProcessorSet,
    PINTERRUPT_HARDWARE_TARGET Target
    )

/*++

Routine Description:

    This routine converts a generic processor set into an interrupt target.
    It may not be possible to target the interrupt at all processors specified,
    this routine will do what it can. On success, at least one processor in
    the set will be targeted. This routine will not target interrupts at a
    processor not mentioned in the set.

    This routine must be run at dispatch level or above.

Arguments:

    ProcessorSet - Supplies a pointer to the processor set.

    Target - Supplies a pointer where the interrupt hardware target will be
        returned.

Return Value:

    Status code.

--*/

{

    ULONG Processor;
    KSTATUS Status;

    RtlZeroMemory(Target, sizeof(INTERRUPT_HARDWARE_TARGET));
    switch (ProcessorSet->Target) {
    case ProcessorTargetAny:

        //
        // If the processor targets are not even initialized then this is a
        // simple uniprocessor machine.
        //

        if (HlProcessorTargets == NULL) {
            Target->Addressing = InterruptAddressingPhysical;
            Target->U.PhysicalId = 0;

        //
        // If the interrupt is targeted at "any" processor, try to set the
        // largest set of processors given the mode.
        //

        } else if (HlLogicalClusteredMode != FALSE) {
            Target->Addressing = InterruptAddressingLogicalClustered;

            ASSERT(HlProcessorTargets[0].Target.Addressing ==
                                          InterruptAddressingLogicalClustered);

            Target->U.Cluster.Id = HlProcessorTargets[0].Target.U.Cluster.Id;
            Target->U.Cluster.Mask =
                                   HlProcessorTargets[0].Target.U.Cluster.Mask;

            //
            // Add every started processor in P0's cluster.
            //

            Processor = 1;
            while ((Processor < HlMaxProcessors) &&
                   ((HlProcessorTargets[Processor].Flags &
                     PROCESSOR_ADDRESSING_FLAG_STARTED) != 0) &&
                   (HlProcessorTargets[Processor].Target.Addressing ==
                    InterruptAddressingLogicalClustered) &&
                   (HlProcessorTargets[Processor].Target.U.Cluster.Id ==
                    Target->U.Cluster.Id)) {

                Target->U.Cluster.Mask |=
                           HlProcessorTargets[Processor].Target.U.Cluster.Mask;

                Processor += 1;
            }

        //
        // Use logical flat mode.
        //

        } else if (HlProcessorTargets[0].Target.Addressing ==
                   InterruptAddressingLogicalFlat) {

            Target->Addressing = InterruptAddressingLogicalFlat;
            Target->U.LogicalFlatId =
                                  HlProcessorTargets[0].Target.U.LogicalFlatId;

            //
            // Add every started processor's logical flat bit into the mix as
            // well.
            //

            Processor = 1;
            while ((Processor < HlMaxProcessors) &&
                   ((HlProcessorTargets[Processor].Flags &
                     PROCESSOR_ADDRESSING_FLAG_STARTED) != 0) &&
                   (HlProcessorTargets[Processor].Target.Addressing ==
                    InterruptAddressingLogicalFlat)) {

                Target->U.LogicalFlatId |=
                          HlProcessorTargets[Processor].Target.U.LogicalFlatId;

                Processor += 1;
            }

        //
        // Use physical mode, just aimed at P0.
        //

        } else {

            ASSERT(HlProcessorTargets[0].Target.Addressing ==
                                                  InterruptAddressingPhysical);

            Target->Addressing = InterruptAddressingPhysical;
            Target->U.PhysicalId = HlProcessorTargets[0].Target.U.PhysicalId;
        }

        break;

    case ProcessorTargetAll:
        Target->Addressing = InterruptAddressingAll;
        break;

    case ProcessorTargetAllExcludingSelf:
        Target->Addressing = InterruptAddressingAllExcludingSelf;
        break;

    case ProcessorTargetSelf:
        Processor = KeGetCurrentProcessorNumber();
        if (HlProcessorTargets == NULL) {
            Target->Addressing = InterruptAddressingSelf;

        } else {
            *Target = HlProcessorTargets[Processor].Target;
        }

        break;

    case ProcessorTargetSingleProcessor:
        if (HlProcessorTargets == NULL) {

            ASSERT(ProcessorSet->U.Number == KeGetCurrentProcessorNumber());

            Target->Addressing = InterruptAddressingSelf;

        } else {
            *Target = HlProcessorTargets[ProcessorSet->U.Number].Target;
        }

        break;

    case ProcessorTargetNone:
    default:
        Status = STATUS_INVALID_PARAMETER;
        goto InterruptConvertProcessorSetToInterruptTargetEnd;
    }

    Status = STATUS_SUCCESS;

InterruptConvertProcessorSetToInterruptTargetEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpInterruptSetupIpiLines (
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine attempts to find interrupt lines suitable for sending IPIs.

Arguments:

    ProcessorNumber - Supplies a pointer to the processor being set up.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_ELIGIBLE_DEVICES if no suitable lines could be found.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    PINTERRUPT_LINE IpiLines;
    ULONG LineCount;
    ULONG LineIndex;
    INTERRUPT_LINE_STATE State;
    KSTATUS Status;
    PROCESSOR_SET Target;

    Controller = HlProcessorTargets[ProcessorNumber].Controller;
    IpiLines = HlProcessorTargets[ProcessorNumber].IpiLine;

    //
    // If this processor has the same controller as P0 (and isn't NULL), copy
    // P0's values.
    //

    if ((ProcessorNumber != 0) &&
        (IpiLines[0].Type == InterruptLineInvalid) &&
        (HlProcessorTargets[ProcessorNumber].Controller ==
         HlProcessorTargets[0].Controller)) {

        RtlCopyMemory(&(HlProcessorTargets[ProcessorNumber].IpiLine),
                      &(HlProcessorTargets[0].IpiLine),
                      sizeof(HlProcessorTargets[0].IpiLine));
    }

    //
    // Determine how many lines this architecture needs.
    //

    LineCount = HlpInterruptGetRequiredIpiLineCount();

    //
    // Loop through each needed IPI line.
    //

    for (LineIndex = 0; LineIndex < LineCount; LineIndex += 1) {

        //
        // Find a line if this processor/controller is just getting set up for
        // the first time.
        //

        if (IpiLines[LineIndex].Type == InterruptLineInvalid) {
            Status = HlpInterruptFindIpiLine(Controller,
                                             &(IpiLines[LineIndex]));

            if (!KSUCCESS(Status)) {
                goto InterruptFindIpiLinesEnd;
            }
        }

        //
        // Configure the line for use.
        //

        Target.Target = ProcessorTargetAll;
        State.Mode = InterruptModeUnknown;
        State.Polarity = InterruptActiveLevelUnknown;
        State.Flags = INTERRUPT_LINE_STATE_FLAG_ENABLED;
        HlpInterruptGetStandardCpuLine(&(State.Output));
        Status = HlpInterruptSetLineState(&(IpiLines[LineIndex]),
                                          &State,
                                          HlIpiKInterrupt[LineIndex],
                                          &Target,
                                          NULL,
                                          0);

        if (!KSUCCESS(Status)) {
            goto InterruptFindIpiLinesEnd;
        }
    }

    Status = STATUS_SUCCESS;

InterruptFindIpiLinesEnd:
    return Status;
}

KSTATUS
HlpInterruptFindIpiLine (
    PINTERRUPT_CONTROLLER Controller,
    PINTERRUPT_LINE Line
    )

/*++

Routine Description:

    This routine attempts to find an interrupt line suitable for sending IPIs.

Arguments:

    Controller - Supplies a pointer to the interrupt controller to search.

    Line - Supplies a pointer that receives the line to use for IPIs.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_ELIGIBLE_DEVICES if no suitable lines could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONG LineCount;
    ULONG LineIndex;
    PINTERRUPT_LINES Lines;
    PINTERRUPT_LINE_INTERNAL_STATE State;

    //
    // Loop through looking for software-only lines.
    //

    CurrentEntry = Controller->LinesHead.Next;
    while (CurrentEntry != &(Controller->LinesHead)) {
        Lines = LIST_VALUE(CurrentEntry, INTERRUPT_LINES, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Lines->Type == InterruptLinesSoftwareOnly) {
            State = Lines->State;
            LineCount = Lines->LineEnd - Lines->LineStart;

            //
            // Loop through looking for a free line in this segment.
            //

            for (LineIndex = 0; LineIndex < LineCount; LineIndex += 1) {
                if ((State[LineIndex].Flags &
                     INTERRUPT_LINE_INTERNAL_STATE_FLAG_RESERVED) == 0) {

                    Line->Type = InterruptLineControllerSpecified;
                    Line->U.Local.Controller = Controller->Identifier;
                    Line->U.Local.Line = Lines->LineStart + LineIndex;
                    return STATUS_SUCCESS;
                }
            }
        }
    }

    Line->Type = InterruptLineInvalid;
    return STATUS_NO_ELIGIBLE_DEVICES;
}


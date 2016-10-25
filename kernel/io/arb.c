/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    arb.c

Abstract:

    This module implements support for system resource arbiters.

Author:

    Evan Green 2-Dec-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"
#include "arb.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this flag to make the arbiter print out all requirement and allocation
// lists.
//

#define ARBITER_DEBUG_PRINT_RESOURCES 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopFinalizeResourceAllocation (
    PDEVICE Device
    );

KSTATUS
IopArbiterAddFreeSpace (
    PRESOURCE_ARBITER Arbiter,
    ULONGLONG FreeSpaceBegin,
    ULONGLONG FreeSpaceLength,
    ULONGLONG FreeSpaceCharacteristics,
    PRESOURCE_ALLOCATION SourcingAllocation,
    ULONGLONG TranslationOffset
    );

KSTATUS
IopArbiterAllocateSpace (
    PARBITER_ALLOCATION_CONTEXT Context,
    UINTN RequirementIndex,
    PRESOURCE_REQUIREMENT Alternative
    );

KSTATUS
IopArbiterInsertEntry (
    PRESOURCE_ARBITER Arbiter,
    ARBITER_SPACE_TYPE SpaceType,
    PDEVICE ClaimingDevice,
    ULONGLONG Allocation,
    ULONGLONG Length,
    ULONGLONG Characteristics,
    ULONG Flags,
    PRESOURCE_REQUIREMENT RootRequirement,
    PARBITER_ENTRY ExistingEntry,
    PARBITER_ENTRY *NewEntry
    );

VOID
IopArbiterFreeEntry (
    PRESOURCE_ARBITER Arbiter,
    PARBITER_ENTRY Entry
    );

VOID
IopArbiterDestroy (
    PRESOURCE_ARBITER Arbiter
    );

PRESOURCE_ARBITER
IopArbiterFindArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType
    );

PARBITER_ENTRY
IopArbiterFindEntry (
    PRESOURCE_ARBITER Arbiter,
    ULONGLONG Allocation,
    BOOL DependentEntryPreferred
    );

VOID
IopArbiterAddRequirement (
    PARBITER_ALLOCATION_CONTEXT Context,
    PRESOURCE_REQUIREMENT Requirement,
    PDEVICE Device
    );

KSTATUS
IopArbiterInitializeAllocationContext (
    PDEVICE Device,
    PARBITER_ALLOCATION_CONTEXT *NewContext
    );

VOID
IopArbiterDestroyAllocationContext (
    PARBITER_ALLOCATION_CONTEXT Context
    );

KSTATUS
IopArbiterSatisfyAllocationContext (
    PARBITER_ALLOCATION_CONTEXT Context
    );

VOID
IopArbiterSortRequirements (
    PARBITER_ALLOCATION_CONTEXT Context
    );

BOOL
IopArbiterIsFirstRequirementHigherPriority (
    PRESOURCE_REQUIREMENT FirstRequirement,
    PRESOURCE_REQUIREMENT SecondRequirement
    );

KSTATUS
IopArbiterRipUpReservedAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    );

KSTATUS
IopArbiterExpandFailingArbiters (
    PARBITER_ALLOCATION_CONTEXT Context
    );

KSTATUS
IopArbiterExpandSpace (
    PRESOURCE_ARBITER Arbiter,
    ULONGLONG AmountNeeded
    );

KSTATUS
IopArbiterLimitResourceHog (
    PARBITER_ALLOCATION_CONTEXT Context
    );

KSTATUS
IopArbiterResizeAllocationContext (
    PARBITER_ALLOCATION_CONTEXT Context,
    ULONG NewDeviceCount,
    ULONG NewRequirementCount
    );

VOID
IopArbiterMarkSelectedConfigurations (
    PARBITER_ALLOCATION_CONTEXT Context
    );

VOID
IopArbiterMatchAllocationsToRequirements (
    PDEVICE Device,
    PULONG RequirementCount
    );

VOID
IopArbiterInitializeResourceAllocation (
    PARBITER_ENTRY ArbiterEntry,
    PRESOURCE_ALLOCATION ResourceAllocation
    );

KSTATUS
IopArbiterCopyAndTranslateResources (
    PRESOURCE_ALLOCATION_LIST BusLocalResources,
    PRESOURCE_ALLOCATION_LIST *ProcessorLocalResources
    );

KSTATUS
IopArbiterTryBootAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    );

KSTATUS
IopArbiterTryBootAllocation (
    PARBITER_ALLOCATION_CONTEXT Context,
    UINTN RequirementIndex
    );

PRESOURCE_ALLOCATION
IopArbiterFindBootAllocationForRequirement (
    PDEVICE Device,
    PRESOURCE_REQUIREMENT Requirement
    );

VOID
IopArbiterClearContextAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    );

VOID
IopArbiterLinkContextAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    );

KSTATUS
IopDeferResourceAllocation (
    PDEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this value to something nonzero in the debugger to enable arbiter debug
// options. See ARBITER_DEBUG_* definitions.
//

ULONG IoArbiterDebugOptions = 0;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
IoCreateResourceArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType
    )

/*++

Routine Description:

    This routine creates a resource arbiter for the given bus device between
    a system resource and the device's children. This function is needed for
    any device whose children access system resources (like physical address
    space) through a window set up by the parent.

Arguments:

    Device - Supplies a pointer to the parent bus device that provides
        resources.

    ResourceType - Supplies the type of resource that the device provides.

Return Value:

    STATUS_SUCCESS if the new arbiter was created.

    STATUS_INVALID_PARAMETER if an invalid resource type was specified.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_ALREADY_INITIALIZED if the device has already has a resource arbiter
    of this type.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    PLIST_ENTRY CurrentEntry;
    PRESOURCE_ARBITER Existing;
    KSTATUS Status;

    Arbiter = NULL;
    if ((ResourceType == ResourceTypeInvalid) ||
        (ResourceType >= ResourceTypeCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreateResourceArbiterEnd;
    }

    //
    // Look for an existing one.
    //

    CurrentEntry = Device->ArbiterListHead.Next;
    while (CurrentEntry != &(Device->ArbiterListHead)) {
        Existing = LIST_VALUE(CurrentEntry, RESOURCE_ARBITER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Existing->ResourceType == ResourceType) {
            Status = STATUS_ALREADY_INITIALIZED;
            goto CreateResourceArbiterEnd;
        }
    }

    //
    // Create the arbiter.
    //

    Arbiter = MmAllocatePagedPool(sizeof(RESOURCE_ARBITER),
                                  ARBITER_ALLOCATION_TAG);

    if (Arbiter == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateResourceArbiterEnd;
    }

    //
    // Initialize and attach the arbiter.
    //

    RtlZeroMemory(Arbiter, sizeof(RESOURCE_ARBITER));
    Arbiter->OwningDevice = Device;
    Arbiter->ResourceType = ResourceType;
    INITIALIZE_LIST_HEAD(&(Arbiter->EntryListHead));
    INSERT_AFTER(&(Arbiter->ListEntry), &(Device->ArbiterListHead));
    Status = STATUS_SUCCESS;

CreateResourceArbiterEnd:
    if (!KSUCCESS(Status)) {
        if (Arbiter != NULL) {
            MmFreePagedPool(Arbiter);
        }
    }

    return Status;
}

KERNEL_API
KSTATUS
IoDestroyResourceArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType
    )

/*++

Routine Description:

    This routine destroys all resource arbiters for the given bus device that
    have the provided resource type.

Arguments:

    Device - Supplies a pointer to the device that owns resource arbitration.

    ResourceType - Supplies the type of resource arbiter that is to be
        destroyed.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ARBITER Arbiter;

    //
    // Find the arbiter. If no arbiter is found, the device is trying to
    // destroy a region without creating an arbiter.
    //

    Arbiter = IopArbiterFindArbiter(Device, ResourceType);
    if (Arbiter == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    ASSERT(Arbiter->OwningDevice == Device);

    //
    // Destroy the arbiter. This will remove the arbiter from the device's
    // arbiter list.
    //

    IopArbiterDestroy(Arbiter);
    return STATUS_SUCCESS;
}

KERNEL_API
KSTATUS
IoAddFreeSpaceToArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType,
    ULONGLONG FreeSpaceBegin,
    ULONGLONG FreeSpaceLength,
    ULONGLONG FreeSpaceCharacteristics,
    PRESOURCE_ALLOCATION SourcingAllocation,
    ULONGLONG TranslationOffset
    )

/*++

Routine Description:

    This routine adds a regions of allocatable space to a previously created
    resource arbiter.

Arguments:

    Device - Supplies a pointer to the device that owns the arbiter (and the
        free space).

    ResourceType - Supplies the resource type that the arbiter can dole out.
        An arbiter of this type must have been created by the device.

    FreeSpaceBegin - Supplies the first address of the free region.

    FreeSpaceLength - Supplies the length of the free region.

    FreeSpaceCharacteristics - Supplies the characteristics of the free
        region.

    SourcingAllocation - Supplies a pointer to the parent resource allocation
        that makes this range possible. This pointer is optional. Supplying
        NULL here implies that the given resource is fixed in nature and
        cannot be expanded.

    TranslationOffset - Supplies the offset that has to be added to all
        doled out allocations on the secondary side to get an address in the
        source allocation space (primary side).
        To recap: SecondaryAddress + TranslationOffset = PrimaryAddress, where
        PrimaryAddress is closer to the CPU complex.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    KSTATUS Status;

    //
    // Find the arbiter. If no arbiter is found, the device is trying to add a
    // region without creating an arbiter.
    //

    Arbiter = IopArbiterFindArbiter(Device, ResourceType);
    if (Arbiter == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (FreeSpaceLength == 0) {
        return STATUS_SUCCESS;
    }

    Status = IopArbiterAddFreeSpace(Arbiter,
                                    FreeSpaceBegin,
                                    FreeSpaceLength,
                                    FreeSpaceCharacteristics,
                                    SourcingAllocation,
                                    TranslationOffset);

    return Status;
}

KERNEL_API
PRESOURCE_ALLOCATION_LIST
IoGetProcessorLocalResources (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine returns the given device's processor local resources.

Arguments:

    Device - Supplies a pointer to the device that owns the resources.

Return Value:

    Returns a pointer to the processor local resource allocation list.

--*/

{

    return Device->ProcessorLocalResources;
}

KSTATUS
IopProcessResourceRequirements (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine attempts to find the best set of resources for a given device.

Arguments:

    Device - Supplies a pointer to the device that will be receiving the
        resource allocation.

Return Value:

    Status code.

--*/

{

    PARBITER_ALLOCATION_CONTEXT Context;
    BOOL Deferred;
    KSTATUS Status;

    Deferred = FALSE;
    if ((IoArbiterDebugOptions & ARBITER_DEBUG_PRINT_RESOURCES) != 0) {
        RtlDebugPrint("Resource Requirements for %s:\n", Device->Header.Name);
        if (Device->ResourceRequirements != NULL) {
            IoDebugPrintResourceConfigurationList(Device->ResourceRequirements);
        }

        RtlDebugPrint("Boot Resources for %s:\n", Device->Header.Name);
        if (Device->BootResources != NULL) {
            IoDebugPrintResourceAllocationList(0, Device->BootResources);
        }
    }

    //
    // Set up an allocation context based on the resource requirements for this
    // device.
    //

    Status = IopArbiterInitializeAllocationContext(Device, &Context);
    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

    if ((Context->DeviceCount == 0) || (Context->RequirementCount == 0)) {
        goto ProcessResourceRequirementsEnd;
    }

    //
    // Try on the boot allocations for size first.
    //

    Status = IopArbiterTryBootAllocations(Context);
    if (KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

    //
    // If the boot allocations did not work and this is the first time through
    // resource assignment, then delay resource assignment of this device until
    // all devices that have boot resources have enumerated. That way devices
    // that happen to come up earlier don't trod on fixed regions of
    // motherboard devices for instance.
    //

    if ((Device->Flags & DEVICE_FLAG_NOT_USING_BOOT_RESOURCES) == 0) {
        Device->Flags |= DEVICE_FLAG_NOT_USING_BOOT_RESOURCES;
        Status = IopDeferResourceAllocation(Device);
        if (KSUCCESS(Status)) {
            Status = STATUS_NOT_READY;
            Deferred = TRUE;
        }

        goto ProcessResourceRequirementsEnd;
    }

    //
    // Start by simply processing the device's requirement list.
    //

    Status = IopArbiterSatisfyAllocationContext(Context);
    if (KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

    //
    // That didn't work out unfortunately. Gather up all reserved allocations
    // (allocations that worked but have not yet been handed out to drivers)
    // from the arbiters that failed.
    //

    Status = IopArbiterRipUpReservedAllocations(Context);
    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

    Status = IopArbiterSatisfyAllocationContext(Context);
    if (KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

    //
    // Unfortunately that wasn't enough either. Attempt to pause all devices
    // with committed resources on the sticky arbiters, rip up all reserved
    // allocations, and try again.
    //

    //
    // That didn't work either. Attempt to expand all failing arbiters.
    //

    Status = IopArbiterExpandFailingArbiters(Context);
    if (!KSUCCESS(Status)) {
        goto ProcessResourceRequirementsEnd;
    }

    //
    // That did all it could, now start knocking devices out of their ideal
    // configuration, and potentially out of the running altogether until
    // there are simply no more devices left.
    // TODO: Also set a timer so that eventually this loop will give up if
    // there are simply too many combinations to try.
    //

    while (Context->DeviceCount != 0) {
        Status = IopArbiterSatisfyAllocationContext(Context);
        if (KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }

        Status = IopArbiterLimitResourceHog(Context);
        if (!KSUCCESS(Status)) {
            goto ProcessResourceRequirementsEnd;
        }
    }

    if (Context->DeviceCount == 0) {
        Status = STATUS_UNSUCCESSFUL;
        goto ProcessResourceRequirementsEnd;
    }

ProcessResourceRequirementsEnd:

    //
    // On success, mark which configuration was chosen for each device.
    //

    if (KSUCCESS(Status)) {
        IopArbiterMarkSelectedConfigurations(Context);
        Status = IopFinalizeResourceAllocation(Device);
        if ((IoArbiterDebugOptions & ARBITER_DEBUG_PRINT_RESOURCES) != 0) {
            RtlDebugPrint("Processor Local Resources for %s:\n",
                          Device->Header.Name);

            if (Device->ProcessorLocalResources != NULL) {
                IoDebugPrintResourceAllocationList(
                                              0,
                                              Device->ProcessorLocalResources);
            }

            RtlDebugPrint("Bus Local Resources for %s:\n", Device->Header.Name);
            if (Device->BusLocalResources != NULL) {
                IoDebugPrintResourceAllocationList(0,
                                                   Device->BusLocalResources);
            }

            RtlDebugPrint("\n");
        }

    } else {
        if ((IoArbiterDebugOptions & ARBITER_DEBUG_PRINT_RESOURCES) != 0) {
            if (Deferred != FALSE) {
                RtlDebugPrint("Deferring resource allocation for %s (0x%x).\n",
                              Device->Header.Name,
                              Device);

            } else {
                RtlDebugPrint("Failed to allocate resource for %s (0x%x). "
                              "Status = %d\n\n",
                              Device->Header.Name,
                              Device,
                              Status);
            }
        }
    }

    if (Context != NULL) {
        IopArbiterDestroyAllocationContext(Context);
    }

    return Status;
}

VOID
IopDestroyArbiterList (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine destroys the arbiter list of the given device.

Arguments:

    Device - Supplies a pointer to a device whose arbiter list is to be
        destroyed.

Return Value:

    None.

--*/

{

    PRESOURCE_ARBITER CurrentArbiter;
    PLIST_ENTRY CurrentEntry;

    //
    // Loop throught the list of arbiters, destroying each one in turn.
    //

    CurrentEntry = Device->ArbiterListHead.Next;
    while (CurrentEntry != &(Device->ArbiterListHead)) {
        CurrentArbiter = LIST_VALUE(CurrentEntry, RESOURCE_ARBITER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        IopArbiterDestroy(CurrentArbiter);
    }

    ASSERT(LIST_EMPTY(&(Device->ArbiterListHead)) != FALSE);
    ASSERT(LIST_EMPTY(&(Device->ArbiterAllocationListHead)) != FALSE);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopFinalizeResourceAllocation (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine cements the resources allocated to a device in preparation for
    starting the device. Once this operation is complete, the device will have
    to be paused to rip up or move its resource allocations.

Arguments:

    Device - Supplies a pointer to the device that is about to be started.

Return Value:

    Status code.

--*/

{

    RESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    PARBITER_ENTRY ArbiterEntry;
    ULONG ArbiterEntryIndex;
    PLIST_ENTRY CurrentAllocation;
    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY CurrentRelatedEntry;
    PRESOURCE_ALLOCATION DependentAllocation;
    PARBITER_ENTRY DependentEntry;
    ULONG DependentEntryIndex;
    ULONG Index;
    PRESOURCE_ALLOCATION OwningAllocation;
    PRESOURCE_ALLOCATION_LIST ProcessorLocalResources;
    ULONG RequirementCount;
    KSTATUS Status;

    AllocationList = NULL;
    ProcessorLocalResources = NULL;

    //
    // If the device didn't ask for resources, then life is easy.
    //

    if (Device->SelectedConfiguration == NULL) {
        Status = STATUS_SUCCESS;
        goto FinalizeResourceAllocationEnd;
    }

    //
    // Rearrange the arbiter allocations to match the order of the resource
    // requirements.
    //

    IopArbiterMatchAllocationsToRequirements(Device, &RequirementCount);
    if (RequirementCount == 0) {
        Status = STATUS_SUCCESS;
        goto FinalizeResourceAllocationEnd;
    }

    //
    // Create the resource allocation buffer, which will hold the array of
    // resource allocations.
    //

    AllocationList = IoCreateResourceAllocationList();
    if (AllocationList == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FinalizeResourceAllocationEnd;
    }

    RtlZeroMemory(&Allocation, sizeof(RESOURCE_ALLOCATION));

    //
    // Loop through the arbiter entry list and convert each entry to a resource
    // allocation.
    //

    CurrentEntry = Device->ArbiterAllocationListHead.Next;
    while (CurrentEntry != &(Device->ArbiterAllocationListHead)) {
        ArbiterEntry = LIST_VALUE(CurrentEntry,
                                  ARBITER_ENTRY,
                                  ConfigurationListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Initialize the resource allocation based on the arbiter entry, and
        // insert it onto the back of the list (maintains the same order).
        //

        IopArbiterInitializeResourceAllocation(ArbiterEntry, &Allocation);
        Status = IoCreateAndAddResourceAllocation(&Allocation, AllocationList);
        if (!KSUCCESS(Status)) {
            goto FinalizeResourceAllocationEnd;
        }

        //
        // Also at this time mark the arbiter entry as permanent.
        //

        ArbiterEntry->Type = ArbiterSpaceAllocated;
    }

    //
    // Copy and translate the bus local resources into processor local
    // resources.
    //

    Status = IopArbiterCopyAndTranslateResources(AllocationList,
                                                 &ProcessorLocalResources);

    if (!KSUCCESS(Status)) {
        goto FinalizeResourceAllocationEnd;
    }

    //
    // Finish up by patching both the allocated bus and processor resources to
    // refer to any owning entries. The relationship goes in the reverse
    // direction of the arbiter relationship (i.e. the same direction as
    // related requirements).
    //

    ArbiterEntryIndex = 0;
    CurrentEntry = Device->ArbiterAllocationListHead.Next;
    while (CurrentEntry != &(Device->ArbiterAllocationListHead)) {
        ArbiterEntry = LIST_VALUE(CurrentEntry,
                                  ARBITER_ENTRY,
                                  ConfigurationListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Skip arbiter entries that have no dependent entries.
        //

        if (ArbiterEntry->DependentEntry == NULL) {
            ArbiterEntryIndex += 1;
            continue;
        }

        //
        // Find the index of the dependent entry.
        //

        DependentEntryIndex = 0;
        CurrentRelatedEntry = Device->ArbiterAllocationListHead.Next;
        while (CurrentRelatedEntry != &(Device->ArbiterAllocationListHead)) {
            DependentEntry = LIST_VALUE(CurrentRelatedEntry,
                                        ARBITER_ENTRY,
                                        ConfigurationListEntry);

            if (ArbiterEntry->DependentEntry == DependentEntry) {
                break;
            }

            CurrentRelatedEntry = CurrentRelatedEntry->Next;
            DependentEntryIndex += 1;
        }

        //
        // The dependent entry isn't in the list of allocated arbiter entries
        // for this device. It is likely that the dependent entry was for an
        // alternate requirement for this device. Or that a different device
        // sharing the resource filled in the dependent entry just in case this
        // device was going to allocate a similarly dependent resource. NULL it
        // out.
        //

        if (CurrentRelatedEntry == &(Device->ArbiterAllocationListHead)) {
            ArbiterEntry->DependentEntry = NULL;
            ArbiterEntryIndex += 1;
            continue;
        }

        //
        // Find the bus and processor allocations for the arbiter entry and
        // dependent entry. If both are found (and they should be), then link
        // the dependent entry's allocation back to the owning arbiter entry's
        // allocation.
        //

        Index = 0;
        OwningAllocation = NULL;
        DependentAllocation = NULL;
        CurrentAllocation = AllocationList->AllocationListHead.Next;
        while (CurrentAllocation != &(AllocationList->AllocationListHead)) {
            if (Index == ArbiterEntryIndex) {
                OwningAllocation = LIST_VALUE(CurrentAllocation,
                                              RESOURCE_ALLOCATION,
                                              ListEntry);
            }

            if (Index == DependentEntryIndex) {
                DependentAllocation = LIST_VALUE(CurrentAllocation,
                                                 RESOURCE_ALLOCATION,
                                                 ListEntry);
            }

            if ((OwningAllocation != NULL) && (DependentAllocation != NULL)) {
                DependentAllocation->OwningAllocation = OwningAllocation;
                break;
            }

            CurrentAllocation = CurrentAllocation->Next;
            Index += 1;
        }

        Index = 0;
        OwningAllocation = NULL;
        DependentAllocation = NULL;
        CurrentAllocation = ProcessorLocalResources->AllocationListHead.Next;
        while (CurrentAllocation !=
               &(ProcessorLocalResources->AllocationListHead)) {

            if (Index == ArbiterEntryIndex) {
                OwningAllocation = LIST_VALUE(CurrentAllocation,
                                              RESOURCE_ALLOCATION,
                                              ListEntry);
            }

            if (Index == DependentEntryIndex) {
                DependentAllocation = LIST_VALUE(CurrentAllocation,
                                                 RESOURCE_ALLOCATION,
                                                 ListEntry);
            }

            if ((OwningAllocation != NULL) && (DependentAllocation != NULL)) {
                DependentAllocation->OwningAllocation = OwningAllocation;
                break;
            }

            CurrentAllocation = CurrentAllocation->Next;
            Index += 1;
        }

        ArbiterEntryIndex += 1;
    }

    Status = STATUS_SUCCESS;

FinalizeResourceAllocationEnd:
    if (!KSUCCESS(Status)) {
        if (AllocationList != NULL) {
            IoDestroyResourceAllocationList(AllocationList);
            AllocationList = NULL;
        }

        if (ProcessorLocalResources != NULL) {
            IoDestroyResourceAllocationList(ProcessorLocalResources);
            ProcessorLocalResources = NULL;
        }
    }

    Device->BusLocalResources = AllocationList;
    Device->ProcessorLocalResources = ProcessorLocalResources;
    return Status;
}

KSTATUS
IopArbiterAddFreeSpace (
    PRESOURCE_ARBITER Arbiter,
    ULONGLONG FreeSpaceBegin,
    ULONGLONG FreeSpaceLength,
    ULONGLONG FreeSpaceCharacteristics,
    PRESOURCE_ALLOCATION SourcingAllocation,
    ULONGLONG TranslationOffset
    )

/*++

Routine Description:

    This routine adds a range of free space to the arbiter, allowing it to dole
    out these resources to child devices.

Arguments:

    Arbiter - Supplies a pointer to the arbiter to add the resources to.

    FreeSpaceBegin - Supplies the beginning value of the free range.

    FreeSpaceLength - Supplies the length of the free space.

    FreeSpaceCharacteristics - Supplies the characteristics of this new free
        space.

    SourcingAllocation - Supplies a pointer to the parent resource allocation
        that makes this range possible. This pointer is optional. Supplying
        NULL here implies that the given resource is fixed in nature and
        cannot be expanded.

    TranslationOffset - Supplies the offset that has to be added to all
        doled out allocations on the secondary side to get an address in the
        source allocation space (primary side).
        To recap: SecondaryAddress + TranslationOffset = PrimaryAddress, where
        PrimaryAddress is closer to the CPU complex.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PARBITER_ENTRY ExistingEntry;
    PARBITER_ENTRY NewEntry;
    PARBITER_ENTRY PreviousEntry;
    KSTATUS Status;

    //
    // Allocate that new entry.
    //

    NewEntry = MmAllocatePagedPool(sizeof(ARBITER_ENTRY),
                                   ARBITER_ALLOCATION_TAG);

    if (NewEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ArbiterAddFreeSpaceEnd;
    }

    RtlZeroMemory(NewEntry, sizeof(ARBITER_ENTRY));
    NewEntry->Type = ArbiterSpaceFree;
    NewEntry->Allocation = FreeSpaceBegin;
    NewEntry->Length = FreeSpaceLength;
    NewEntry->Characteristics = FreeSpaceCharacteristics;
    NewEntry->FreeCharacteristics = FreeSpaceCharacteristics;
    NewEntry->SourceAllocation = SourcingAllocation;
    NewEntry->TranslationOffset = TranslationOffset;

    //
    // Find the proper place for this entry in the list.
    //

    ExistingEntry = NULL;
    CurrentEntry = Arbiter->EntryListHead.Next;
    while (CurrentEntry != &(Arbiter->EntryListHead)) {
        ExistingEntry = LIST_VALUE(CurrentEntry, ARBITER_ENTRY, ListEntry);
        if (ExistingEntry->Allocation >= NewEntry->Allocation) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Check for overlaps.
    //

    if (CurrentEntry == &(Arbiter->EntryListHead)) {
        INSERT_BEFORE(&(NewEntry->ListEntry), &(Arbiter->EntryListHead));

    } else {

        //
        // Check to see if this should be merged with the previous entry. If so,
        // free the previous entry and expand this new one to cover it.
        //

        if (ExistingEntry->ListEntry.Previous != &(Arbiter->EntryListHead)) {
            PreviousEntry = LIST_VALUE(ExistingEntry->ListEntry.Previous,
                                       ARBITER_ENTRY,
                                       ListEntry);

            if ((PreviousEntry->Type == ArbiterSpaceFree) &&
                (PreviousEntry->Characteristics == NewEntry->Characteristics) &&
                (PreviousEntry->SourceAllocation ==
                 NewEntry->SourceAllocation) &&
                (PreviousEntry->TranslationOffset ==
                 NewEntry->TranslationOffset) &&
                (PreviousEntry->Allocation + PreviousEntry->Length >=
                 NewEntry->Allocation)) {

                NewEntry->Length += NewEntry->Allocation -
                                    PreviousEntry->Allocation;

                if (PreviousEntry->Length > NewEntry->Length) {
                    NewEntry->Length = PreviousEntry->Length;
                }

                NewEntry->Allocation = PreviousEntry->Allocation;
                LIST_REMOVE(&(PreviousEntry->ListEntry));

                ASSERT(PreviousEntry->ConfigurationListEntry.Next == NULL);

                MmFreePagedPool(PreviousEntry);
            }
        }

        //
        // Check to see if this should be merged with the next entry. If so,
        // free up the new entry and expand the existing one to cover it.
        //

        if ((ExistingEntry->Type == ArbiterSpaceFree) &&
            (ExistingEntry->Characteristics == NewEntry->Characteristics) &&
            (ExistingEntry->SourceAllocation == NewEntry->SourceAllocation) &&
            (ExistingEntry->TranslationOffset == NewEntry->TranslationOffset) &&
            (NewEntry->Allocation + NewEntry->Length >=
             ExistingEntry->Allocation)) {

            ExistingEntry->Length += ExistingEntry->Allocation -
                                     NewEntry->Allocation;

            if (NewEntry->Length > ExistingEntry->Length) {
                ExistingEntry->Length = NewEntry->Length;
            }

            ExistingEntry->Allocation = NewEntry->Allocation;
            MmFreePagedPool(NewEntry);
            NewEntry = NULL;
        }

        //
        // If the new entry is still around, add it to the list before the
        // existing one.
        //

        if (NewEntry != NULL) {

            //
            // Check to see if it should be shrunk.
            //

            if (NewEntry->Allocation + NewEntry->Length >
                ExistingEntry->Allocation) {

                NewEntry->Length = ExistingEntry->Allocation -
                                   NewEntry->Allocation;

                ASSERT(NewEntry->Length != 0);
            }

            INSERT_BEFORE(&(NewEntry->ListEntry), CurrentEntry);
        }
    }

    Status = STATUS_SUCCESS;

ArbiterAddFreeSpaceEnd:
    return Status;
}

KSTATUS
IopArbiterAllocateSpace (
    PARBITER_ALLOCATION_CONTEXT Context,
    UINTN RequirementIndex,
    PRESOURCE_REQUIREMENT Alternative
    )

/*++

Routine Description:

    This routine attempts to allocate space from an arbiter.

Arguments:

    Context - Supplies a pointer to the arbiter allocation context.

    RequirementIndex - Supplies the index of the requirement. If an alternative
        requirement is provided, then the routine will attempt to satisfy said
        alternative, but will set the corresponding requirement field of the
        arbiter entry to that of the requirement index. Thus, the allocation
        always points at the first requirement, not the potentially alternative
        requirement being satisfied.

    Alternative - Supplies an optional pointer to an alternative resource
        requirement to satisfy.

Return Value:

    Status code.

--*/

{

    ULONGLONG AllocationEnd;
    BOOL AllowOverlaps;
    PRESOURCE_ARBITER Arbiter;
    PARBITER_ENTRY ArbiterEntry;
    PARBITER_ENTRY CompatibleSpace;
    PLIST_ENTRY CurrentEntry;
    PDEVICE Device;
    ULONG Index;
    PARBITER_ENTRY NewAllocation;
    PARBITER_ENTRY OwningRequirementEntry;
    ULONGLONG PotentialAllocation;
    PARBITER_ENTRY RequiredSpace;
    PRESOURCE_REQUIREMENT Requirement;
    PARBITER_ALLOCATION_REQUIREMENT RequirementData;
    PRESOURCE_REQUIREMENT RootRequirement;
    KSTATUS Status;

    NewAllocation = NULL;

    //
    // If an alternative requirement was supplied, then use it.
    //

    RootRequirement = Context->Requirements[RequirementIndex].Requirement;
    if (Alternative != NULL) {

        ASSERT(Alternative->Type == RootRequirement->Type);

        Requirement = Alternative;

    } else {
        Requirement = RootRequirement;
    }

    RequirementData = &(Context->Requirements[RequirementIndex]);
    Device = IOP_ARBITER_GET_DEVICE(Context, RequirementData);
    Arbiter = IOP_ARBITER_GET_ARBITER(Context, RequirementData);

    ASSERT(Arbiter != NULL);

    //
    // If this requirement has a owning requirement, then search for the
    // allocated arbiter entry associated with it.
    //

    OwningRequirementEntry = NULL;
    if (Requirement->OwningRequirement != NULL) {
        for (Index = 0; Index < Context->RequirementCount; Index += 1) {
            ArbiterEntry = Context->Requirements[Index].Allocation;
            if ((ArbiterEntry != NULL) &&
                (ArbiterEntry->CorrespondingRequirement ==
                 Requirement->OwningRequirement)) {

                OwningRequirementEntry = ArbiterEntry;
                break;
            }
        }

        //
        // If the owning requirement has an allocated arbiter entry and that
        // arbiter entry has a dependent arbiter allocation, then this
        // requirement needs to use those exact resources. The owning arbiter
        // entry picked up the dependent entry from another device's use of the
        // same region.
        //

        if ((OwningRequirementEntry != NULL) &&
            (OwningRequirementEntry->DependentEntry != NULL)) {

            RequiredSpace = OwningRequirementEntry->DependentEntry;

            ASSERT(RequiredSpace->Type != ArbiterSpaceFree);
            ASSERT(RequiredSpace->CorrespondingRequirement->Type ==
                   Requirement->Type);

            //
            // If the space does not match the requirement, then it cannot be
            // used and something is wrong.
            //

            if ((RequiredSpace->Characteristics !=
                 Requirement->Characteristics) ||
                ((Requirement->Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) ||
                ((RequiredSpace->Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) ||
                (Requirement->Length != RequiredSpace->Length)) {

                Status = STATUS_RESOURCE_IN_USE;
                goto ArbiterAllocateSpaceEnd;
            }

            //
            // The required allocation must have the correct alignment.
            //

            if (IS_ALIGNED(RequiredSpace->Allocation,
                           Requirement->Alignment) == FALSE) {

                Status = STATUS_RESOURCE_IN_USE;
                goto ArbiterAllocateSpaceEnd;
            }

            //
            // The allocation must also fit within the required bounds.
            //

            PotentialAllocation = RequiredSpace->Allocation;
            AllocationEnd = PotentialAllocation + Requirement->Length;
            if ((PotentialAllocation < Requirement->Minimum) ||
                (AllocationEnd > Requirement->Maximum)) {

                Status = STATUS_RESOURCE_IN_USE;
                goto ArbiterAllocateSpaceEnd;
            }

            //
            // The required space works! Create a new arbiter entry.
            //

            Status = IopArbiterInsertEntry(Arbiter,
                                           ArbiterSpaceReserved,
                                           Device,
                                           PotentialAllocation,
                                           Requirement->Length,
                                           Requirement->Characteristics,
                                           Requirement->Flags,
                                           RootRequirement,
                                           RequiredSpace,
                                           &NewAllocation);

            goto ArbiterAllocateSpaceEnd;
        }
    }

    //
    // Zero-length requirements have no issue with overlap. Just allocate an
    // arbiter entry.
    //

    if (Requirement->Length == 0) {
        Status = IopArbiterInsertEntry(Arbiter,
                                       ArbiterSpaceReserved,
                                       Device,
                                       Requirement->Minimum,
                                       0,
                                       Requirement->Characteristics,
                                       Requirement->Flags,
                                       RootRequirement,
                                       NULL,
                                       &NewAllocation);

        goto ArbiterAllocateSpaceEnd;
    }

    //
    // Loop through every entry in the arbiter twice, first looking for only
    // free space and then allowing overlaps.
    //

    AllowOverlaps = FALSE;
    while (TRUE) {
        CurrentEntry = Arbiter->EntryListHead.Next;
        while (CurrentEntry != &(Arbiter->EntryListHead)) {
            CompatibleSpace = LIST_VALUE(CurrentEntry,
                                         ARBITER_ENTRY,
                                         ListEntry);

            CurrentEntry = CurrentEntry->Next;

            //
            // If the entry isn't free, then it probably won't work. The only
            // supported overlaps are two entries that both satisfy the given
            // criteria:
            //
            //     1) Same characteristics.
            //     2) Same base works for both.
            //     3) Same length.
            //

            if (CompatibleSpace->Type != ArbiterSpaceFree) {
                if (AllowOverlaps == FALSE) {
                    continue;
                }

                if ((CompatibleSpace->Length != Requirement->Length) ||
                    (CompatibleSpace->Characteristics !=
                     Requirement->Characteristics) ||
                    ((Requirement->Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) ||
                    ((CompatibleSpace->Flags & RESOURCE_FLAG_NOT_SHAREABLE) !=
                     0)) {

                    continue;
                }

                if (IS_ALIGNED(CompatibleSpace->Allocation,
                               Requirement->Alignment) == FALSE) {

                    continue;
                }
            }

            //
            // Skip it if it's below the minimum.
            //

            if ((CompatibleSpace->Allocation + CompatibleSpace->Length) <=
                Requirement->Minimum) {

                continue;
            }

            //
            // If characteristics are set in the free space, then those
            // characteristics are assumed to be serious and need to be matched.
            //

            if ((CompatibleSpace->Characteristics &
                 Requirement->Characteristics) !=
                CompatibleSpace->Characteristics) {

                continue;
            }

            //
            // Attempt to fit an allocation in here.
            //

            if (CompatibleSpace->Allocation > Requirement->Minimum) {
                PotentialAllocation = CompatibleSpace->Allocation;

            } else {
                PotentialAllocation = Requirement->Minimum;
            }

            PotentialAllocation = ALIGN_RANGE_UP(PotentialAllocation,
                                                 Requirement->Alignment);

            //
            // If this is not a free entry, the allocations had better be equal
            // (or else releasing the allocation won't work properly.
            //

            ASSERT((CompatibleSpace->Type == ArbiterSpaceFree) ||
                   (PotentialAllocation == CompatibleSpace->Allocation));

            AllocationEnd = PotentialAllocation + Requirement->Length;

            //
            // If the end here is beyond the maximum, then no allocation in the
            // arbiter will work.
            //

            if (AllocationEnd > Requirement->Maximum) {
                Status = STATUS_UNSUCCESSFUL;
                goto ArbiterAllocateSpaceEnd;
            }

            //
            // If the allocation doesn't fit, move on to the next arbiter entry.
            //

            if (AllocationEnd >
                (CompatibleSpace->Allocation + CompatibleSpace->Length)) {

                continue;
            }

            //
            // The allocation fits! Create a new arbiter entry.
            //

            Status = IopArbiterInsertEntry(Arbiter,
                                           ArbiterSpaceReserved,
                                           Device,
                                           PotentialAllocation,
                                           Requirement->Length,
                                           Requirement->Characteristics,
                                           Requirement->Flags,
                                           RootRequirement,
                                           CompatibleSpace,
                                           &NewAllocation);

            goto ArbiterAllocateSpaceEnd;
        }

        //
        // If the list has already been searched allowing overlaps, then it's
        // time to bail out. No arbiter space was found to be satisfactory.
        //

        if (AllowOverlaps != FALSE) {
            break;
        }

        //
        // Next time around, allow this allocation to overlap with existing
        // resources.
        //

        AllowOverlaps = TRUE;
    }

    Status = STATUS_RESOURCE_IN_USE;

ArbiterAllocateSpaceEnd:
    if (KSUCCESS(Status)) {
        if (OwningRequirementEntry != NULL) {
            OwningRequirementEntry->DependentEntry = NewAllocation;
        }

        Context->Requirements[RequirementIndex].Allocation = NewAllocation;
    }

    return Status;
}

KSTATUS
IopArbiterInsertEntry (
    PRESOURCE_ARBITER Arbiter,
    ARBITER_SPACE_TYPE SpaceType,
    PDEVICE ClaimingDevice,
    ULONGLONG Allocation,
    ULONGLONG Length,
    ULONGLONG Characteristics,
    ULONG Flags,
    PRESOURCE_REQUIREMENT RootRequirement,
    PARBITER_ENTRY ExistingEntry,
    PARBITER_ENTRY *NewEntry
    )

/*++

Routine Description:

    This routine inserts an entry into the arbiter. It does not perform any
    checks for resource conflicts, so it is only for use by the arbiter. An
    external function would want to do much more involved conflict checking.

Arguments:

    Arbiter - Supplies a pointer to the arbiter to add the new entry to.

    SpaceType - Supplies the type of arbiter space this entry should be set
        to.

    ClaimingDevice - Supplies a pointer to the device that will be using this
        region.

    Allocation - Supplies the allocation base.

    Length - Supplies the length of the allocation.

    Characteristics - Supplies the allocation characteristics.

    Flags - Supplies the flags for the allocation.

    RootRequirement - Supplies the requirment to set as the "corresponding
        requirement" of this arbiter entry, used to connect arbiter allocations
        with resource requirements.

    ExistingEntry - Supplies an optional pointer to the entry that currently
        exists for the range that is to be given to the new entry. This may be
        a free entry or an allocated, yet shareable, entry.

    NewEntry - Supplies a pointer where a pointer to the new entry will be
        returned. This memory is managed by the arbiter.

Return Value:

    Status code.

--*/

{

    ULONGLONG AllocationEnd;
    PLIST_ENTRY CurrentEntry;
    PARBITER_ENTRY Leftovers;
    PARBITER_ENTRY NewAllocation;
    PARBITER_ENTRY NextEntry;
    KSTATUS Status;

    AllocationEnd = Allocation + Length;

    //
    // Create and initialize a new arbiter entry.
    //

    NewAllocation = MmAllocatePagedPool(sizeof(ARBITER_ENTRY),
                                        ARBITER_ALLOCATION_TAG);

    if (NewAllocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ArbiterInsertEntryEnd;
    }

    RtlZeroMemory(NewAllocation, sizeof(ARBITER_ENTRY));
    NewAllocation->Type = ArbiterSpaceReserved;
    NewAllocation->Device = ClaimingDevice;
    NewAllocation->Allocation = Allocation;
    NewAllocation->Length = Length;
    NewAllocation->Characteristics = Characteristics;
    NewAllocation->Flags = Flags;
    NewAllocation->CorrespondingRequirement = RootRequirement;
    if (ExistingEntry != NULL) {
        NewAllocation->FreeCharacteristics = ExistingEntry->FreeCharacteristics;
        NewAllocation->SourceAllocation = ExistingEntry->SourceAllocation;
        NewAllocation->TranslationOffset = ExistingEntry->TranslationOffset;

        ASSERT((ExistingEntry->Type != ArbiterSpaceFree) ||
               (ExistingEntry->DependentEntry == NULL));

        NewAllocation->DependentEntry = ExistingEntry->DependentEntry;
    }

    if (ExistingEntry != NULL) {

        //
        // If there is leftover space, allocate an entry for that.
        //

        if ((ExistingEntry->Type == ArbiterSpaceFree) &&
            (AllocationEnd <
             (ExistingEntry->Allocation + ExistingEntry->Length))) {

            Leftovers = MmAllocatePagedPool(sizeof(ARBITER_ENTRY),
                                            ARBITER_ALLOCATION_TAG);

            if (Leftovers == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto ArbiterInsertEntryEnd;
            }

            RtlCopyMemory(Leftovers, ExistingEntry, sizeof(ARBITER_ENTRY));
            Leftovers->Allocation = AllocationEnd;
            Leftovers->Length = ExistingEntry->Allocation +
                                ExistingEntry->Length -
                                AllocationEnd;

            INSERT_AFTER(&(Leftovers->ListEntry), &(ExistingEntry->ListEntry));
        }

        INSERT_AFTER(&(NewAllocation->ListEntry), &(ExistingEntry->ListEntry));

        //
        // Shrink the old free entry, and remove it if it shrinks all the way
        // to zero.
        //

        if (ExistingEntry->Type == ArbiterSpaceFree) {
            ExistingEntry->Length = Allocation - ExistingEntry->Allocation;
            if (ExistingEntry->Length == 0) {
                LIST_REMOVE(&(ExistingEntry->ListEntry));
                MmFreePagedPool(ExistingEntry);
            }
        }

    //
    // Find the right spot to insert this new entry.
    //

    } else {
        CurrentEntry = Arbiter->EntryListHead.Next;
        while (CurrentEntry != &(Arbiter->EntryListHead)) {
            NextEntry = LIST_VALUE(CurrentEntry, ARBITER_ENTRY, ListEntry);
            if (NextEntry->Allocation >= Allocation) {
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (CurrentEntry == &(Arbiter->EntryListHead)) {
            INSERT_BEFORE(&(NewAllocation->ListEntry),
                          &(Arbiter->EntryListHead));

        } else {
            INSERT_BEFORE(&(NewAllocation->ListEntry), &(NextEntry->ListEntry));
        }
    }

    Status = STATUS_SUCCESS;

ArbiterInsertEntryEnd:
    if (!KSUCCESS(Status)) {
        if (NewAllocation != NULL) {
            MmFreePagedPool(NewAllocation);
            NewAllocation = NULL;
        }
    }

    *NewEntry = NewAllocation;
    return Status;
}

VOID
IopArbiterFreeEntry (
    PRESOURCE_ARBITER Arbiter,
    PARBITER_ENTRY Entry
    )

/*++

Routine Description:

    This routine frees an arbiter entry.

Arguments:

    Arbiter - Supplies a pointer to the arbiter where the allocation belongs.

    Entry - Supplies a pointer to the arbiter allocation.

Return Value:

    Status code.

--*/

{

    ULONGLONG AllocationBegin;
    ULONGLONG Characteristics;
    PARBITER_ENTRY NextEntry;
    PARBITER_ENTRY OverlappingEntry;
    PARBITER_ENTRY PreviousEntry;

    ASSERT(Entry->Type != ArbiterSpaceFree);

    AllocationBegin = Entry->Allocation;
    Characteristics = Entry->FreeCharacteristics;
    PreviousEntry = LIST_VALUE(Entry->ListEntry.Previous,
                               ARBITER_ENTRY,
                               ListEntry);

    NextEntry = LIST_VALUE(Entry->ListEntry.Next, ARBITER_ENTRY, ListEntry);
    LIST_REMOVE(&(Entry->ListEntry));
    if (Entry->Length == 0) {
        return;
    }

    //
    // Attempt to find an entry that overlapped with this one. If such an entry
    // exists, don't patch up free space into this region, since some other
    // allocation is still there. Just make this allocation disappear.
    //

    OverlappingEntry = IopArbiterFindEntry(Arbiter, AllocationBegin, FALSE);
    if (OverlappingEntry != NULL) {

        ASSERT(OverlappingEntry->Type != ArbiterSpaceFree);

        MmFreePagedPool(Entry);
        return;
    }

    //
    // Put the entry back on the list, as it makes it easier for the
    // coalescing code.
    //

    INSERT_AFTER(&(Entry->ListEntry), &(PreviousEntry->ListEntry));

    //
    // If the previous entry is free and comes up to meet this allocation, then
    // expand that allocation. Remove and free this allocation.
    //

    if ((Entry->ListEntry.Previous != &(Arbiter->EntryListHead)) &&
        (PreviousEntry->Type == ArbiterSpaceFree) &&
        (PreviousEntry->SourceAllocation == Entry->SourceAllocation) &&
        (PreviousEntry->TranslationOffset == Entry->TranslationOffset) &&
        (PreviousEntry->Characteristics == Characteristics) &&
        (PreviousEntry->Allocation + PreviousEntry->Length ==
         Entry->Allocation)) {

        PreviousEntry->Length += Entry->Length;
        LIST_REMOVE(&(Entry->ListEntry));
        MmFreePagedPool(Entry);

        //
        // Set the current entry to that previous entry that expanded out.
        //

        Entry = PreviousEntry;
    }

    //
    // See if the next allocation can swallow up this one.
    //

    if ((Entry->ListEntry.Next != &(Arbiter->EntryListHead)) &&
        (NextEntry->Type == ArbiterSpaceFree) &&
        (NextEntry->SourceAllocation == Entry->SourceAllocation) &&
        (NextEntry->TranslationOffset == Entry->TranslationOffset) &&
        (NextEntry->Characteristics == Characteristics) &&
        (Entry->Allocation + Entry->Length == NextEntry->Allocation)) {

        NextEntry->Length += Entry->Length;
        NextEntry->Allocation = Entry->Allocation;
        LIST_REMOVE(&(Entry->ListEntry));
        MmFreePagedPool(Entry);
        Entry = NULL;
    }

    //
    // If the entry is not already marked as free, mark it as such now.
    //

    if ((Entry != NULL) && (Entry->Type != ArbiterSpaceFree)) {
        Entry->Device = NULL;
        Entry->CorrespondingRequirement = NULL;
        Entry->Characteristics = Characteristics;
        Entry->Flags = 0;
        Entry->Type = ArbiterSpaceFree;
        Entry->DependentEntry = NULL;
    }

    return;
}

VOID
IopArbiterDestroy (
    PRESOURCE_ARBITER Arbiter
    )

/*++

Routine Description:

    This routine destroys an individual resource arbiter, removing it from its
    list of arbiters.

Arguments:

    Arbiter - Supplies a pointer to the arbiter that is to be destroyed.

Return Value:

    None.

--*/

{

    PARBITER_ENTRY ArbiterEntry;
    PLIST_ENTRY CurrentEntry;

    //
    // In the destruction path, there is no point to free any of the arbiter
    // entries, just loop here and nuke them.
    //

    CurrentEntry = Arbiter->EntryListHead.Next;
    while (CurrentEntry != &(Arbiter->EntryListHead)) {
        ArbiterEntry = LIST_VALUE(CurrentEntry, ARBITER_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        LIST_REMOVE(&(ArbiterEntry->ConfigurationListEntry));
        LIST_REMOVE(&(ArbiterEntry->ListEntry));
        MmFreePagedPool(ArbiterEntry);
    }

    //
    // Destroy the arbiter itself.
    //

    LIST_REMOVE(&(Arbiter->ListEntry));
    MmFreePagedPool(Arbiter);
    return;
}

PRESOURCE_ARBITER
IopArbiterFindArbiter (
    PDEVICE Device,
    RESOURCE_TYPE ResourceType
    )

/*++

Routine Description:

    This routine searches for the arbiter of the given resouce type that is
    attached to the given device.

Arguments:

    Device - Supplies a pointer to the device whose arbiter list is to be
        searched.

    ResourceType - Supplies the resource type of the requested arbiter.

Return Value:

    A pointer to a resource arbiter on success. NULL on failure.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    PRESOURCE_ARBITER CurrentArbiter;
    PLIST_ENTRY CurrentEntry;

    //
    // Find the arbiter with the provided resource type.
    //

    Arbiter = NULL;
    CurrentEntry = Device->ArbiterListHead.Next;
    while (CurrentEntry != &(Device->ArbiterListHead)) {
        CurrentArbiter = LIST_VALUE(CurrentEntry, RESOURCE_ARBITER, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (CurrentArbiter->ResourceType == ResourceType) {
            Arbiter = CurrentArbiter;
            break;
        }
    }

    return Arbiter;
}

PARBITER_ENTRY
IopArbiterFindEntry (
    PRESOURCE_ARBITER Arbiter,
    ULONGLONG Allocation,
    BOOL DependentEntryPreferred
    )

/*++

Routine Description:

    This routine attempts to find an arbiter entry for the given allocation.
    If there are more than one arbiter entries covering the same range, this
    routine will simply find the first one it comes across. If the dependent
    entry parameter is set to TRUE, then it will find the first one with a
    dependent entry filled in. If no such entry exists, then it will return the
    first one.

Arguments:

    Arbiter - Supplies a pointer to the arbiter to search.

    Allocation - Supplies a pointer to the allocation value to check.

    DependentEntryPreferred - Supplies a boolean indicating whether or not the
        search should prioritize finding any entry that has a valid dependent
        entry field.

Return Value:

    Returns a pointer to the first arbiter entry that covers the given
    allocation value.

    NULL if no arbiter entry covers the given value.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PARBITER_ENTRY Entry;
    PARBITER_ENTRY FirstEntry;

    FirstEntry = NULL;
    CurrentEntry = Arbiter->EntryListHead.Next;
    while (CurrentEntry != &(Arbiter->EntryListHead)) {
        Entry = LIST_VALUE(CurrentEntry, ARBITER_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Entry->Allocation <= Allocation) &&
            (Entry->Allocation + Entry->Length > Allocation)) {

            //
            // Return this entry if it doesn't need to have a dependent entry
            // or it has a dependent entry.
            //

            if ((DependentEntryPreferred == FALSE) ||
                (Entry->DependentEntry != NULL)) {

                return Entry;
            }

            if (FirstEntry == NULL) {
                FirstEntry = Entry;
            }

        //
        // If a non-satisfying entry was found after the satisfying entries
        // have been checked, return the first entry found.
        //

        } else if (FirstEntry != NULL) {
            break;
        }
    }

    return FirstEntry;
}

VOID
IopArbiterAddRequirement (
    PARBITER_ALLOCATION_CONTEXT Context,
    PRESOURCE_REQUIREMENT Requirement,
    PDEVICE Device
    )

/*++

Routine Description:

    This routine adds a requirement to the arbiter allocation context. The
    caller must have previously called resize arbiter context so that the
    arrays are large enough.

Arguments:

    Context - Supplies a pointer to the initialized allocation context.

    Requirement - Supplies a pointer to the requirement to add.

    Device - Supplies a pointer to the device that generated the requirement.

Return Value:

    None. An arbiter is always found since the root device has empty ones for
    each type.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    UINTN ArbiterIndex;
    UINTN DeviceIndex;
    ULONG EmptySlot;
    PLIST_ENTRY FirstConfigurationListEntry;
    PDEVICE Provider;
    PARBITER_ALLOCATION_REQUIREMENT RequirementData;
    ULONG RequirementIndex;

    RequirementIndex = Context->RequirementCount;
    RequirementData = &(Context->Requirements[RequirementIndex]);
    RequirementData->Requirement = Requirement;
    RequirementData->Allocation = NULL;

    ASSERT((ARBITER_TYPE)Requirement->Type < ArbiterTypeCount);

    //
    // The arbiter comes from the device's parent unless a different provider
    // was explicitly given.
    //

    Provider = Device->ParentDevice;
    if (Requirement->Provider != NULL) {
        Provider = Requirement->Provider;
    }

    //
    // Walk up the chain of parents to find the arbiter for this requirement.
    //

    while (TRUE) {
        Arbiter = IopArbiterFindArbiter(Provider, Requirement->Type);

        //
        // If an arbiter was found, see if it's already in the arbiter array.
        // Insert if not, or just set the index if it is.
        //

        if (Arbiter != NULL) {
            for (ArbiterIndex = 0;
                 ArbiterIndex < Context->ArbiterCount;
                 ArbiterIndex += 1) {

                if (Context->ArbiterData[ArbiterIndex].Arbiter == Arbiter) {
                    break;
                }
            }

            if (ArbiterIndex == Context->ArbiterCount) {
                Context->ArbiterData[ArbiterIndex].Arbiter = Arbiter;
                Context->ArbiterCount = ArbiterIndex + 1;
            }

            RequirementData->ArbiterIndex = ArbiterIndex;
            break;
        }

        Provider = Provider->ParentDevice;

        ASSERT(Provider != NULL);
    }

    //
    // Also find the device index for this requirement, or add the device if
    // it's new. Try to reuse empty slots from removed devices.
    //

    EmptySlot = Context->DeviceCount;
    for (DeviceIndex = 0;
         DeviceIndex < Context->DeviceCount;
         DeviceIndex += 1) {

        if (Context->Device[DeviceIndex] == Device) {
            break;
        }

        if (Context->Device[DeviceIndex] == NULL) {
            EmptySlot = DeviceIndex;
        }
    }

    if (DeviceIndex == Context->DeviceCount) {
        DeviceIndex = EmptySlot;
        Context->Device[EmptySlot] = Device;
        FirstConfigurationListEntry =
            Device->ResourceRequirements->RequirementListListHead.Next;

        Context->CurrentDeviceConfiguration[EmptySlot] =
                                        LIST_VALUE(FirstConfigurationListEntry,
                                                   RESOURCE_REQUIREMENT_LIST,
                                                   ListEntry);

        if (EmptySlot == Context->DeviceCount) {
            Context->DeviceCount += 1;
        }
    }

    RequirementData->DeviceIndex = DeviceIndex;
    RequirementData->Allocation = NULL;
    Context->RequirementCount += 1;
    return;
}

KSTATUS
IopArbiterInitializeAllocationContext (
    PDEVICE Device,
    PARBITER_ALLOCATION_CONTEXT *NewContext
    )

/*++

Routine Description:

    This routine creates and initializes an arbiter allocation context, and
    seeds it with the resource requirements for the most optimal configuration
    for the given device.

Arguments:

    Device - Supplies a pointer to the new kid on the block, the device trying
        to get resources.

    NewContext - Supplies a pointer that on success will receive a pointer to
        the arbiter allocation context. The caller is responsible for
        destroying this context.

Return Value:

    Status code.

--*/

{

    PARBITER_ALLOCATION_CONTEXT Context;
    PLIST_ENTRY CurrentEntry;
    PRESOURCE_REQUIREMENT_LIST FirstConfiguration;
    PLIST_ENTRY FirstConfigurationListEntry;
    PRESOURCE_REQUIREMENT Requirement;
    ULONG RequirementCount;
    KSTATUS Status;

    //
    // Create an arbiter allocation context.
    //

    Context = MmAllocatePagedPool(sizeof(ARBITER_ALLOCATION_CONTEXT),
                                  ARBITER_ALLOCATION_TAG);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ArbiterInitializeAllocationContextEnd;
    }

    RtlZeroMemory(Context, sizeof(ARBITER_ALLOCATION_CONTEXT));
    if ((Device->ResourceRequirements == NULL) ||
        (LIST_EMPTY(&(Device->ResourceRequirements->RequirementListListHead)) !=
         FALSE)) {

        Status = STATUS_SUCCESS;
        goto ArbiterInitializeAllocationContextEnd;
    }

    FirstConfigurationListEntry =
                    Device->ResourceRequirements->RequirementListListHead.Next;

    FirstConfiguration = LIST_VALUE(FirstConfigurationListEntry,
                                    RESOURCE_REQUIREMENT_LIST,
                                    ListEntry);

    //
    // Loop through once to find out how many requirements are in this list.
    //

    RequirementCount = 0;
    CurrentEntry = FirstConfiguration->RequirementListHead.Next;
    while (CurrentEntry != &(FirstConfiguration->RequirementListHead)) {
        RequirementCount += 1;
        CurrentEntry = CurrentEntry->Next;
    }

    if (RequirementCount == 0) {
        Status = STATUS_SUCCESS;
        goto ArbiterInitializeAllocationContextEnd;
    }

    //
    // Create the arrays.
    //

    Status = IopArbiterResizeAllocationContext(Context, 1, RequirementCount);
    if (!KSUCCESS(Status)) {
        goto ArbiterInitializeAllocationContextEnd;
    }

    //
    // Initialize the requirement list.
    //

    CurrentEntry = FirstConfiguration->RequirementListHead.Next;
    while (CurrentEntry != &(FirstConfiguration->RequirementListHead)) {
        Requirement = LIST_VALUE(CurrentEntry, RESOURCE_REQUIREMENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        IopArbiterAddRequirement(Context, Requirement, Device);
    }

    Status = STATUS_SUCCESS;

ArbiterInitializeAllocationContextEnd:
    if (!KSUCCESS(Status)) {
        if (Context != NULL) {
            if (Context->Device != NULL) {
                MmFreePagedPool(Context->Device);
            }

            if (Context->Requirements != NULL) {
                MmFreePagedPool(Context->Requirements);
            }

            MmFreePagedPool(Context);
            Context = NULL;
        }
    }

    *NewContext = Context;
    return Status;
}

VOID
IopArbiterDestroyAllocationContext (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys an arbiter allocation context.

Arguments:

    Context - Supplies a pointer to the arbiter allocation context to release.

Return Value:

    None.

--*/

{

    if (Context->Device != NULL) {
        MmFreePagedPool(Context->Device);
    }

    if (Context->Requirements != NULL) {
        MmFreePagedPool(Context->Requirements);
    }

    MmFreePagedPool(Context);
    return;
}

KSTATUS
IopArbiterSatisfyAllocationContext (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to allocate all the resource requirements currently
    in the allocation context.

Arguments:

    Context - Supplies a pointer to the arbiter allocation context to attempt
        to satisfy.

Return Value:

    STATUS_SUCCESS if all resource requirements were satisfied.

    STATUS_UNSUCCESSFUL if not all resource requirements could be satisfied.

    Other error codes on other failures.

--*/

{

    BOOL AllocationFailed;
    PRESOURCE_ARBITER Arbiter;
    PARBITER_ALLOCATION_ARBITER_DATA ArbiterData;
    ULONG ArbiterIndex;
    PRESOURCE_REQUIREMENT CurrentAlternative;
    PRESOURCE_REQUIREMENT Requirement;
    PARBITER_ALLOCATION_REQUIREMENT RequirementData;
    ULONG RequirementIndex;
    KSTATUS Status;

    AllocationFailed = FALSE;
    for (ArbiterIndex = 0;
         ArbiterIndex < Context->ArbiterCount;
         ArbiterIndex += 1) {

        Context->ArbiterData[ArbiterIndex].AmountNotAllocated = 0;
    }

    //
    // Prioritize the requirements.
    //

    IopArbiterSortRequirements(Context);

    //
    // Loop through every requirement in the array and attempt to create an
    // allocation for it.
    //

    for (RequirementIndex = 0;
         RequirementIndex < Context->RequirementCount;
         RequirementIndex += 1) {

        //
        // Prefer the boot allocations.
        //

        Status = IopArbiterTryBootAllocation(Context, RequirementIndex);
        if (KSUCCESS(Status)) {
            continue;
        }

        RequirementData = &(Context->Requirements[RequirementIndex]);
        Requirement = RequirementData->Requirement;
        ArbiterData = IOP_GET_ARBITER_DATA(Context, RequirementData);
        Arbiter = ArbiterData->Arbiter;

        ASSERT(Arbiter != NULL);

        //
        // Loop through every possible alternative in the list trying to make
        // one stick.
        //

        Status = STATUS_UNSUCCESSFUL;
        CurrentAlternative = Requirement;
        while (CurrentAlternative != NULL) {
            Status = IopArbiterAllocateSpace(Context,
                                             RequirementIndex,
                                             CurrentAlternative);

            if (KSUCCESS(Status)) {
                break;
            }

            CurrentAlternative = IoGetNextResourceRequirementAlternative(
                                                           Requirement,
                                                           CurrentAlternative);
        }

        //
        // If nothing stuck, remember that something failed, and by how much.
        //

        if (!KSUCCESS(Status)) {
            AllocationFailed = TRUE;
            ArbiterData->AmountNotAllocated += Requirement->Length;
        }
    }

    //
    // If not all allocations were made, free them all.
    //

    if (AllocationFailed != FALSE) {
        IopArbiterClearContextAllocations(Context);
        Status = STATUS_UNSUCCESSFUL;

    //
    // If the allocations were successful, link them into the device's arbiter
    // entry list. Don't worry about the order for now.
    //

    } else {
        IopArbiterLinkContextAllocations(Context);
        Status = STATUS_SUCCESS;
    }

    return Status;
}

VOID
IopArbiterSortRequirements (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine sorts all the resource requirements in an allocation context,
    prioritizing them by their ratio of requirement to possible spots.

Arguments:

    Context - Supplies a pointer to the arbiter allocation context to sort.

Return Value:

    None.

--*/

{

    ULONG FastIndex;
    PARBITER_ALLOCATION_REQUIREMENT FirstRequirement;
    BOOL InWrongOrder;
    PARBITER_ALLOCATION_REQUIREMENT SecondRequirement;
    ULONG SlowIndex;
    ARBITER_ALLOCATION_REQUIREMENT Swap;

    if (Context->RequirementCount == 0) {
        return;
    }

    //
    // Surely you can implement a better sort than this ridiculously lame one.
    //

    for (SlowIndex = 0;
         SlowIndex < Context->RequirementCount - 1;
         SlowIndex += 1) {

        FirstRequirement = &(Context->Requirements[SlowIndex]);
        for (FastIndex = SlowIndex + 1;
             FastIndex < Context->RequirementCount;
             FastIndex += 1) {

            SecondRequirement = &(Context->Requirements[FastIndex]);

            //
            // The two are in the wrong order if the second requirement is
            // greater than the first.
            //

            InWrongOrder = IopArbiterIsFirstRequirementHigherPriority(
                                                SecondRequirement->Requirement,
                                                FirstRequirement->Requirement);

            //
            // Swap the entries if they're in the wrong order.
            //

            if (InWrongOrder != FALSE) {
                RtlCopyMemory(&Swap,
                              FirstRequirement,
                              sizeof(ARBITER_ALLOCATION_REQUIREMENT));

                RtlCopyMemory(FirstRequirement,
                              SecondRequirement,
                              sizeof(ARBITER_ALLOCATION_REQUIREMENT));

                RtlCopyMemory(SecondRequirement,
                              &Swap,
                              sizeof(ARBITER_ALLOCATION_REQUIREMENT));

                FirstRequirement = SecondRequirement;
            }
        }
    }

    return;
}

BOOL
IopArbiterIsFirstRequirementHigherPriority (
    PRESOURCE_REQUIREMENT FirstRequirement,
    PRESOURCE_REQUIREMENT SecondRequirement
    )

/*++

Routine Description:

    This routine compares two resource requirements and determines if the first
    requirement is a higher priority allocation to satisfy than the second.

Arguments:

    FirstRequirement - Supplies a pointer to the requirement to be compared.

    SecondRequirement - Supplies a pointer to the requirement to compare with.

Return Value:

    TRUE if the first requirement is of higher priority than the second.

    FALSE if the first requirement is of equal or lesser priority than the
    second.

--*/

{

    ULONGLONG Alignment;
    ULONGLONG FirstRequirementPossibilities;
    ULONGLONG SecondRequirementPossibilities;

    //
    // Sort first by requirement type. The lower the type value the higher the
    // priority.
    //

    if (FirstRequirement->Type != SecondRequirement->Type) {
        if (FirstRequirement->Type < SecondRequirement->Type) {
            return TRUE;
        }

        return FALSE;
    }

    //
    // Get each requirement's priority. The priority is based on the number of
    // different positions this requirement could take in it's range of
    // possibilities.
    // TODO: Add alternatives into the mix here.
    //

    Alignment = FirstRequirement->Alignment;
    if (Alignment == 0) {
        Alignment = 1;
    }

    FirstRequirementPossibilities =
                (FirstRequirement->Maximum - FirstRequirement->Minimum -
                 FirstRequirement->Length) / Alignment;

    Alignment = SecondRequirement->Alignment;
    if (Alignment == 0) {
        Alignment = 1;
    }

    SecondRequirementPossibilities =
                (SecondRequirement->Maximum - SecondRequirement->Minimum -
                 SecondRequirement->Length) / Alignment;

    if (FirstRequirementPossibilities < SecondRequirementPossibilities) {
        return TRUE;
    }

    return FALSE;
}

KSTATUS
IopArbiterRipUpReservedAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine surveys all the arbiters in the given context that have failed.
    It rips up all reserved allocations in those arbiters (removing them from
    the device's arbiter entry list), and adds the corresponding resource
    requirements to the context. The hope is that by completely rearranging
    all the furniture in the room there will be space for that one more chair.

Arguments:

    Context - Supplies a pointer to the arbiter allocation context to attempt
        to satisfy.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    ULONG ArbiterIndex;
    PLIST_ENTRY CurrentEntry;
    ULONG DeviceCount;
    PARBITER_ENTRY Entry;
    ULONG RequirementCount;
    KSTATUS Status;

    //
    // Loop through all arbiters once to figure out the new total number of
    // requirements and devices involved. One might think that a nice
    // optimization might be to avoid ripping up arbiters that aren't failing.
    // Unfortunately this is not possible, since if a previously uninvolved
    // device's allocations get ripped up, ALL of its allocations need to be
    // ripped up (since it might get adjusted down a configuration).
    //

    RequirementCount = Context->RequirementCount;
    DeviceCount = Context->DeviceCount;
    for (ArbiterIndex = 0;
         ArbiterIndex < Context->ArbiterCount;
         ArbiterIndex += 1) {

        Arbiter = Context->ArbiterData[ArbiterIndex].Arbiter;
        if (Arbiter == NULL) {
            continue;
        }

        //
        // Loop through every entry in the arbiter.
        //

        CurrentEntry = Arbiter->EntryListHead.Next;
        while (CurrentEntry != &(Arbiter->EntryListHead)) {
            Entry = LIST_VALUE(CurrentEntry, ARBITER_ENTRY, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Entry->Type != ArbiterSpaceReserved) {
                continue;
            }

            RequirementCount += 1;

            //
            // Assume that every new requirement belongs to a unique device.
            // This is almost certainly too much, but will simply result in an
            // array that is allocated to be a bit too big.
            //

            DeviceCount += 1;
        }
    }

    //
    // Resize the arrays to fit the new stuff.
    //

    Status = IopArbiterResizeAllocationContext(Context,
                                               DeviceCount,
                                               RequirementCount);

    if (!KSUCCESS(Status)) {
        goto ArbiterRipUpReservedAllocationsEnd;
    }

    //
    // Loop through the arbiters again now that everything is prepared for the
    // new allocations. Release anything in the arbiters that hasn't yet been
    // given to a device driver.
    //

    for (ArbiterIndex = 0;
         ArbiterIndex < Context->ArbiterCount;
         ArbiterIndex += 1) {

        Arbiter = Context->ArbiterData[ArbiterIndex].Arbiter;
        if (Arbiter == NULL) {
            continue;
        }

        //
        // Loop through every entry in the arbiter.
        //

        CurrentEntry = Arbiter->EntryListHead.Next;
        while (CurrentEntry != &(Arbiter->EntryListHead)) {
            Entry = LIST_VALUE(CurrentEntry, ARBITER_ENTRY, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Entry->Type != ArbiterSpaceReserved) {
                continue;
            }

            IopArbiterAddRequirement(Context,
                                     Entry->CorrespondingRequirement,
                                     Entry->Device);

            //
            // Remove the entry.
            //

            LIST_REMOVE(&(Entry->ConfigurationListEntry));
            IopArbiterFreeEntry(Arbiter, Entry);
        }
    }

    Status = STATUS_SUCCESS;

ArbiterRipUpReservedAllocationsEnd:
    return Status;
}

KSTATUS
IopArbiterExpandFailingArbiters (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to create more space in every arbiter that does not
    have enough space.

Arguments:

    Context - Supplies a pointer to the allocation context to work with.

Return Value:

    Status code.

--*/

{

    ULONGLONG AmountNeeded;
    PRESOURCE_ARBITER Arbiter;
    PARBITER_ALLOCATION_ARBITER_DATA ArbiterData;
    ULONG ArbiterIndex;
    ULONGLONG ArbiterSize;
    PLIST_ENTRY CurrentEntry;
    PARBITER_ENTRY Entry;

    for (ArbiterIndex = 0;
         ArbiterIndex < Context->ArbiterCount;
         ArbiterIndex += 1) {

        ArbiterData = &(Context->ArbiterData[ArbiterIndex]);

        //
        // If the arbiter doesn't have a problem, don't touch it.
        //

        if (ArbiterData->AmountNotAllocated == 0) {
            continue;
        }

        Arbiter = ArbiterData->Arbiter;

        ASSERT(Arbiter != NULL);

        //
        // Loop through every entry in the arbiter.
        //

        ArbiterSize = 0;
        CurrentEntry = Arbiter->EntryListHead.Next;
        while (CurrentEntry != &(Arbiter->EntryListHead)) {
            Entry = LIST_VALUE(CurrentEntry, ARBITER_ENTRY, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if (Entry->Type == ArbiterSpaceFree) {
                ArbiterSize += Entry->Length;
                continue;
            }

            break;
        }

        //
        // If there were allocations in the arbiter, then it cannot be
        // resized.
        //

        if (CurrentEntry != &(Arbiter->EntryListHead)) {
            continue;
        }

        //
        // Ask for more space, the old size plus double the amount not
        // allocated.
        //

        AmountNeeded = ArbiterSize + (ArbiterData->AmountNotAllocated * 2);
        IopArbiterExpandSpace(Arbiter, AmountNeeded);
    }

    return STATUS_SUCCESS;
}

KSTATUS
IopArbiterExpandSpace (
    PRESOURCE_ARBITER Arbiter,
    ULONGLONG AmountNeeded
    )

/*++

Routine Description:

    This routine asks the arbiter's device for more space to put into the
    arbiter. On success, the arbiter will have more free space.

Arguments:

    Arbiter - Supplies a pointer to the arbiter in need.

    AmountNeeded - Supplies the amount of additional space needed.

Return Value:

    Status code.

--*/

{

    return STATUS_NOT_IMPLEMENTED;
}

KSTATUS
IopArbiterLimitResourceHog (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine starts making compromises for the sake of device resource
    allocation. It finds the most congested resource, looks for the biggest
    potential consumer of that resource, and knocks that device down a
    configuration.

Arguments:

    Context - Supplies a pointer to the context causing the resource squeeze.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    PARBITER_ALLOCATION_ARBITER_DATA ArbiterData;
    ULONG ArbiterIndex;
    ULONGLONG BiggestRequirementAmount;
    ULONG BiggestRequirementIndex;
    PRESOURCE_REQUIREMENT_LIST Configuration;
    PLIST_ENTRY CurrentEntry;
    PDEVICE Device;
    ULONG DeviceIndex;
    ULONG EndRequirementIndex;
    PLIST_ENTRY NextConfigurationListEntry;
    BOOL RemoveDevice;
    PRESOURCE_REQUIREMENT Requirement;
    ULONG RequirementCount;
    PARBITER_ALLOCATION_REQUIREMENT RequirementData;
    ULONG RequirementIndex;
    KSTATUS Status;
    PRESOURCE_ARBITER TightestArbiter;
    ULONGLONG TightestArbiterAmount;

    //
    // Find the tightest arbiter.
    //

    TightestArbiter = NULL;
    TightestArbiterAmount = 0;
    for (ArbiterIndex = 0;
         ArbiterIndex < Context->ArbiterCount;
         ArbiterIndex += 1) {

        ArbiterData = &(Context->ArbiterData[ArbiterIndex]);
        Arbiter = ArbiterData->Arbiter;
        if (ArbiterData->AmountNotAllocated > TightestArbiterAmount) {
            TightestArbiterAmount = ArbiterData->AmountNotAllocated;
            TightestArbiter = Arbiter;

            ASSERT(Arbiter != NULL);
        }
    }

    ASSERT(TightestArbiter != NULL);

    //
    // Find the biggest requirement for that arbiter that's not already in the
    // last configuration.
    //

    RemoveDevice = FALSE;
    BiggestRequirementAmount = 0;
    BiggestRequirementIndex = -1;
    for (RequirementIndex = 0;
         RequirementIndex < Context->RequirementCount;
         RequirementIndex += 1) {

        RequirementData = &(Context->Requirements[RequirementIndex]);
        Device = IOP_ARBITER_GET_DEVICE(Context, RequirementData);
        Requirement = RequirementData->Requirement;
        DeviceIndex = RequirementData->DeviceIndex;

        ASSERT(DeviceIndex < Context->DeviceCount);

        //
        // Skip if it's the last configuration.
        //

        if (Context->CurrentDeviceConfiguration[DeviceIndex]->ListEntry.Next ==
            &(Device->ResourceRequirements->RequirementListListHead)) {

            continue;
        }

        //
        // Remember if it's the new big guy.
        //

        if (Requirement->Length > BiggestRequirementAmount) {
            BiggestRequirementAmount = Requirement->Length;
            BiggestRequirementIndex = RequirementIndex;
        }
    }

    //
    // If there is no big guy, then everyone is at their worst configuration.
    // Find a device to knock out of the race.
    //

    if (BiggestRequirementIndex == -1) {
        RemoveDevice = TRUE;
        BiggestRequirementAmount = 0;
        BiggestRequirementIndex = -1;
        for (RequirementIndex = 0;
             RequirementIndex < Context->RequirementCount;
             RequirementIndex += 1) {

            RequirementData = &(Context->Requirements[RequirementIndex]);
            Device = IOP_ARBITER_GET_DEVICE(Context, RequirementData);
            Requirement = RequirementData->Requirement;
            DeviceIndex = RequirementData->DeviceIndex;

            ASSERT(DeviceIndex < Context->DeviceCount);

            //
            // Remember if it's the new big guy.
            //

            if (Requirement->Length > BiggestRequirementAmount) {
                BiggestRequirementAmount = Requirement->Length;
                BiggestRequirementIndex = RequirementIndex;
            }
        }
    }

    ASSERT(BiggestRequirementIndex != -1);

    //
    // Remove all requirements associated with the device at its old
    // configuration.
    //

    RequirementData = &(Context->Requirements[BiggestRequirementIndex]);
    Device = IOP_ARBITER_GET_DEVICE(Context, RequirementData);
    DeviceIndex = RequirementData->DeviceIndex;
    for (RequirementIndex = 0;
         RequirementIndex < Context->RequirementCount;
         NOTHING) {

        //
        // If this is the magic device's requirement, move the requirement from
        // the end of the array on top of this one.
        //

        RequirementData = &(Context->Requirements[RequirementIndex]);
        if (IOP_ARBITER_GET_DEVICE(Context, RequirementData) == Device) {

            ASSERT(RequirementData->Allocation == NULL);

            EndRequirementIndex = Context->RequirementCount - 1;
            if (EndRequirementIndex != RequirementIndex) {
                RtlCopyMemory(RequirementData,
                              &(Context->Requirements[EndRequirementIndex]),
                              sizeof(ARBITER_ALLOCATION_REQUIREMENT));
            }

            Context->RequirementCount -= 1;

        //
        // Only advance to the next index if that requirement wasn't just
        // replaced.
        //

        } else {
            RequirementIndex += 1;
        }
    }

    ASSERT(DeviceIndex < Context->DeviceCount);

    //
    // If it's getting desperate, remove the device itself.
    //

    if (RemoveDevice != FALSE) {
        Context->Device[DeviceIndex] = NULL;
        Context->CurrentDeviceConfiguration[DeviceIndex] = NULL;

    //
    // Notch the configuration down a tick, and add all those requirements.
    //

    } else {
        Configuration = Context->CurrentDeviceConfiguration[DeviceIndex];
        NextConfigurationListEntry = Configuration->ListEntry.Next;

        ASSERT(NextConfigurationListEntry !=
               &(Device->ResourceRequirements->RequirementListListHead));

        Configuration = LIST_VALUE(NextConfigurationListEntry,
                                   RESOURCE_REQUIREMENT_LIST,
                                   ListEntry);

        Context->CurrentDeviceConfiguration[DeviceIndex] = Configuration;

        //
        // Loop through the configuration once to determine how many
        // requirements there are.
        //

        RequirementCount = 0;
        CurrentEntry = Configuration->RequirementListHead.Next;
        while (CurrentEntry != &(Configuration->RequirementListHead)) {
            RequirementCount += 1;
            CurrentEntry = CurrentEntry->Next;
        }

        //
        // Resize the arrays.
        //

        Status = IopArbiterResizeAllocationContext(
                                 Context,
                                 Context->DeviceCount,
                                 Context->RequirementCount + RequirementCount);

        if (!KSUCCESS(Status)) {
            goto ArbiterLimitResourceHogEnd;
        }

        //
        // Loop through again and add the resource requirements.
        //

        CurrentEntry = Configuration->RequirementListHead.Next;
        while (CurrentEntry != &(Configuration->RequirementListHead)) {
            Requirement = LIST_VALUE(CurrentEntry,
                                     RESOURCE_REQUIREMENT,
                                     ListEntry);

            CurrentEntry = CurrentEntry->Next;
            IopArbiterAddRequirement(Context, Requirement, Device);
        }
    }

    Status = STATUS_SUCCESS;

ArbiterLimitResourceHogEnd:
    return Status;
}

KSTATUS
IopArbiterResizeAllocationContext (
    PARBITER_ALLOCATION_CONTEXT Context,
    ULONG NewDeviceCount,
    ULONG NewRequirementCount
    )

/*++

Routine Description:

    This routine resizes the appropriate arrays in the given arbiter allocation
    context. This routine allocates new arrays of the given size, copies the
    old contents over, and frees the old arrays. It does not modify the
    device or requirement count variables.

Arguments:

    Context - Supplies a pointer to the context to adjust.

    NewDeviceCount - Supplies the new device count.

    NewRequirementCount - Supplies the new resource requirement count.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    UINTN CopySize;
    PARBITER_ALLOCATION_ARBITER_DATA NewArbiterDataArray;
    PRESOURCE_REQUIREMENT_LIST *NewCurrentDeviceConfigurationArray;
    PDEVICE *NewDeviceArray;
    PARBITER_ALLOCATION_REQUIREMENT NewRequirementArray;
    ULONG OldRequirementCount;
    KSTATUS Status;

    NewDeviceArray = NULL;
    NewRequirementArray = NULL;

    //
    // Allocate the new arrays in the context.
    //

    AllocationSize = (sizeof(PDEVICE) * NewDeviceCount) +
                     (sizeof(PRESOURCE_REQUIREMENT_LIST) * NewDeviceCount);

    NewDeviceArray = MmAllocatePagedPool(AllocationSize,
                                         ARBITER_ALLOCATION_TAG);

    if (NewDeviceArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ArbiterResizeAllocationContextEnd;
    }

    RtlZeroMemory(NewDeviceArray, AllocationSize);
    NewCurrentDeviceConfigurationArray =
                (PRESOURCE_REQUIREMENT_LIST *)(NewDeviceArray + NewDeviceCount);

    AllocationSize = (sizeof(ARBITER_ALLOCATION_REQUIREMENT) *
                      NewRequirementCount) +
                     (sizeof(ARBITER_ALLOCATION_ARBITER_DATA) *
                      NewRequirementCount);

    NewRequirementArray = MmAllocatePagedPool(AllocationSize,
                                              ARBITER_ALLOCATION_TAG);

    if (NewRequirementArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ArbiterResizeAllocationContextEnd;
    }

    RtlZeroMemory(NewRequirementArray, AllocationSize);
    NewArbiterDataArray =
                       (PARBITER_ALLOCATION_ARBITER_DATA)(NewRequirementArray +
                                                          NewRequirementCount);

    //
    // Copy the old arrays into the new arrays. The allocations are not
    // copied because they're all NULL at this point.
    //

    if (Context->Device != NULL) {
        RtlCopyMemory(NewDeviceArray,
                      Context->Device,
                      Context->DeviceCount * sizeof(PDEVICE));

        RtlCopyMemory(
                    NewCurrentDeviceConfigurationArray,
                    Context->CurrentDeviceConfiguration,
                    Context->DeviceCount * sizeof(PRESOURCE_REQUIREMENT_LIST));

        MmFreePagedPool(Context->Device);
    }

    if (Context->Requirements != NULL) {
        OldRequirementCount = Context->RequirementCount;
        CopySize = OldRequirementCount * sizeof(ARBITER_ALLOCATION_REQUIREMENT);
        RtlCopyMemory(NewRequirementArray, Context->Requirements, CopySize);
        CopySize = Context->ArbiterCount *
                   sizeof(ARBITER_ALLOCATION_ARBITER_DATA);

        RtlCopyMemory(NewArbiterDataArray, Context->ArbiterData, CopySize);
        MmFreePagedPool(Context->Requirements);
    }

    //
    // Replace the old arrays with the newly improved bigger arrays. Leave
    // the sizes alone as they will be expanded as they go.
    //

    Context->Device = NewDeviceArray;
    Context->CurrentDeviceConfiguration = NewCurrentDeviceConfigurationArray;
    Context->Requirements = NewRequirementArray;
    Context->ArbiterData = NewArbiterDataArray;
    Status = STATUS_SUCCESS;

ArbiterResizeAllocationContextEnd:
    if (!KSUCCESS(Status)) {
        if (NewDeviceArray != NULL) {
            MmFreePagedPool(NewDeviceArray);
        }

        if (NewRequirementArray != NULL) {
            MmFreePagedPool(NewRequirementArray);
        }
    }

    return Status;
}

VOID
IopArbiterMarkSelectedConfigurations (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine marks which resource configuration was chosen in each device
    involved.

    Note: By adjusting resource configurations of devices that had gotten ripped
          up, there is an assumption that a device and all of its siblings
          share the same set of arbiters. If this is not true, then the arbiters
          will return invalid configurations. For example, a first device is
          reserved resources from its first configuration, then a second device
          with a different set of arbiters comes in, the first device's
          allocations are ripped up (but only those in the same set of
          arbiters that the second device is working with), and then the
          resource configuration of the first device is adjusted down. This
          would result in the first device being given some resources from its
          first configuration (from the arbiters that didn't overlap with the
          second device) and other resource from a lower configuration.

Arguments:

    Context - Supplies a pointer to the allocation context that was just
        satisfied.

Return Value:

    None.

--*/

{

    PDEVICE Device;
    ULONG DeviceIndex;

    for (DeviceIndex = 0;
         DeviceIndex < Context->DeviceCount;
         DeviceIndex += 1) {

        Device = Context->Device[DeviceIndex];
        if (Device == NULL) {
            continue;
        }

        Device->SelectedConfiguration =
                              Context->CurrentDeviceConfiguration[DeviceIndex];
    }

    return;
}

VOID
IopArbiterMatchAllocationsToRequirements (
    PDEVICE Device,
    PULONG RequirementCount
    )

/*++

Routine Description:

    This routine rearranges the list of the device's arbiter entries so that
    they are in the same order as the device's resource requirement list.
    It can also optionally return the number of resource requirements the
    device has.

Arguments:

    Device - Supplies a pointer to the device.

    RequirementCount - Supplies a pointer where the number of requirements
        will be returned.

Return Value:

    Status code.

--*/

{

    PARBITER_ENTRY CurrentAllocation;
    PLIST_ENTRY CurrentAllocationEntry;
    PRESOURCE_REQUIREMENT CurrentRequirement;
    PLIST_ENTRY CurrentRequirementEntry;
    ULONG NumberOfRequirements;
    PLIST_ENTRY PreviousAllocationEntry;
    PRESOURCE_REQUIREMENT_LIST RequirementList;

    RequirementList = Device->SelectedConfiguration;
    NumberOfRequirements = 0;

    //
    // Loop through every requirement in the requirement list.
    //

    PreviousAllocationEntry = &(Device->ArbiterAllocationListHead);
    CurrentRequirementEntry = RequirementList->RequirementListHead.Next;
    while (CurrentRequirementEntry != &(RequirementList->RequirementListHead)) {
        CurrentRequirement = LIST_VALUE(CurrentRequirementEntry,
                                        RESOURCE_REQUIREMENT,
                                        ListEntry);

        CurrentRequirementEntry = CurrentRequirementEntry->Next;
        NumberOfRequirements += 1;

        //
        // Loop through the remaining arbiter allocations to find the one that
        // corresponds to this requirement.
        //

        CurrentAllocationEntry = PreviousAllocationEntry->Next;
        while (CurrentAllocationEntry != &(Device->ArbiterAllocationListHead)) {
            CurrentAllocation = LIST_VALUE(CurrentAllocationEntry,
                                           ARBITER_ENTRY,
                                           ConfigurationListEntry);

            if (CurrentAllocation->CorrespondingRequirement ==
                CurrentRequirement) {

                LIST_REMOVE(CurrentAllocationEntry);
                INSERT_AFTER(CurrentAllocationEntry, PreviousAllocationEntry);
                break;
            }

            CurrentAllocationEntry = CurrentAllocationEntry->Next;
        }

        ASSERT(CurrentAllocationEntry != &(Device->ArbiterAllocationListHead));

        PreviousAllocationEntry = PreviousAllocationEntry->Next;
    }

    if (RequirementCount != NULL) {
        *RequirementCount = NumberOfRequirements;
    }

    return;
}

VOID
IopArbiterInitializeResourceAllocation (
    PARBITER_ENTRY ArbiterEntry,
    PRESOURCE_ALLOCATION ResourceAllocation
    )

/*++

Routine Description:

    This routine initializes a resource allocation based on an arbiter entry.

Arguments:

    ResourceType - Supplies the resource type of the allocation to initialize.

    ArbiterEntry - Supplies a pointer to the arbiter entry to use as a template
        for the resource allocation.

    ResourceAllocation - Supplies a pointer where the resource allocation
        corresponding to the given arbiter entry will be returned.

Return Value:

    None.

--*/

{

    PRESOURCE_REQUIREMENT Requirement;

    Requirement = ArbiterEntry->CorrespondingRequirement;
    ResourceAllocation->Type = Requirement->Type;
    ResourceAllocation->Allocation = ArbiterEntry->Allocation;
    ResourceAllocation->Length = ArbiterEntry->Length;
    ResourceAllocation->Characteristics = ArbiterEntry->Characteristics;
    ResourceAllocation->Flags = Requirement->Flags;
    ResourceAllocation->Data = Requirement->Data;
    ResourceAllocation->DataSize = Requirement->DataSize;
    ResourceAllocation->Provider = Requirement->Provider;
    return;
}

KSTATUS
IopArbiterCopyAndTranslateResources (
    PRESOURCE_ALLOCATION_LIST BusLocalResources,
    PRESOURCE_ALLOCATION_LIST *ProcessorLocalResources
    )

/*++

Routine Description:

    This routine translates a set of resources from bus local resources to
    processor local resources.

Arguments:

    BusLocalResources - Supplies a pointer to the bus local resources to
        translate from.

    ProcessorLocalResources - Supplies a pointer where a newly allocated list
        of processor local resources will be returned. The caller is
        responsible for destroying this resource list once it is returned.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    KSTATUS Status;
    RESOURCE_ALLOCATION TranslatedResource;
    PRESOURCE_ALLOCATION_LIST TranslatedResources;

    TranslatedResources = NULL;
    if (BusLocalResources == NULL) {
        Status = STATUS_SUCCESS;
        goto ArbiterCopyAndTranslateResourcesEnd;
    }

    //
    // Create a new resource allocation list.
    //

    TranslatedResources = IoCreateResourceAllocationList();
    if (TranslatedResources == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ArbiterCopyAndTranslateResourcesEnd;
    }

    Allocation = IoGetNextResourceAllocation(BusLocalResources, NULL);
    while (Allocation != NULL) {

        //
        // Create a local copy of the resource and translate it.
        //

        RtlCopyMemory(&TranslatedResource,
                      Allocation,
                      sizeof(RESOURCE_ALLOCATION));

        //
        // TODO: Find the arbiter entry associated with this resource and
        // apply the translation.
        //

        //
        // Create a copy of the resource.
        //

        Status = IoCreateAndAddResourceAllocation(&TranslatedResource,
                                                  TranslatedResources);

        if (!KSUCCESS(Status)) {
            goto ArbiterCopyAndTranslateResourcesEnd;
        }

        //
        // Get the next allocation.
        //

        Allocation = IoGetNextResourceAllocation(BusLocalResources, Allocation);
    }

    Status = STATUS_SUCCESS;

ArbiterCopyAndTranslateResourcesEnd:
    if (!KSUCCESS(Status)) {
        if (TranslatedResources != NULL) {
            IoDestroyResourceAllocationList(TranslatedResources);
            TranslatedResources = NULL;
        }
    }

    *ProcessorLocalResources = TranslatedResources;
    return Status;
}

KSTATUS
IopArbiterTryBootAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to use the boot allocations for a device.

Arguments:

    Context - Supplies a pointer to the arbiter allocation context.

Return Value:

    STATUS_SUCCESS if all the boot allocations were successfully reserved in
    the arbiters.

    Other errors on failure.

--*/

{

    ULONG RequirementIndex;
    KSTATUS Status;

    //
    // Loop through all the requirements.
    //

    for (RequirementIndex = 0;
         RequirementIndex < Context->RequirementCount;
         RequirementIndex += 1) {

        Status = IopArbiterTryBootAllocation(Context, RequirementIndex);
        if (!KSUCCESS(Status)) {
            goto ArbiterTryBootAllocationsEnd;
        }
    }

    //
    // Everything worked, link the context allocations to their requirement's
    // devices.
    //

    IopArbiterLinkContextAllocations(Context);
    Status = STATUS_SUCCESS;

ArbiterTryBootAllocationsEnd:
    if (!KSUCCESS(Status)) {
        IopArbiterClearContextAllocations(Context);
    }

    return Status;
}

KSTATUS
IopArbiterTryBootAllocation (
    PARBITER_ALLOCATION_CONTEXT Context,
    UINTN RequirementIndex
    )

/*++

Routine Description:

    This routine attempts to use the boot allocation for a particular
    requirement.

Arguments:

    Context - Supplies a pointer to the arbiter allocation context.

    RequirementIndex - Supplies the index of the requirement to try and
        satisfy with a boot allocation.

Return Value:

    STATUS_SUCCESS if all the boot allocations were successfully reserved in
    the arbiters.

    Other errors on failure.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    PARBITER_ENTRY ArbiterEntry;
    PRESOURCE_ALLOCATION BootAllocation;
    BOOL Conflict;
    PDEVICE Device;
    PARBITER_ENTRY NewEntry;
    PRESOURCE_REQUIREMENT Requirement;
    PARBITER_ALLOCATION_REQUIREMENT RequirementData;
    KSTATUS Status;

    RequirementData = &(Context->Requirements[RequirementIndex]);
    Requirement = RequirementData->Requirement;
    Device = IOP_ARBITER_GET_DEVICE(Context, RequirementData);
    Arbiter = IOP_ARBITER_GET_ARBITER(Context, RequirementData);

    ASSERT(Arbiter != NULL);

    BootAllocation = IopArbiterFindBootAllocationForRequirement(Device,
                                                                Requirement);

    //
    // If there's no boot allocation for this requirement or the boot
    // allocation doesn't satisfy the requirement, attempt to satisfy it
    // with something else.
    //

    if ((BootAllocation == NULL) ||
        (BootAllocation->Length < Requirement->Length)) {

        Status = IopArbiterAllocateSpace(Context, RequirementIndex, NULL);
        goto ArbiterTryBootAllocationEnd;
    }

    //
    // Requirements satisfied by boot allocations should not have related
    // requirements.
    //

    ASSERT(Requirement->OwningRequirement == NULL);

    //
    // Find out what's in the arbiter at this location.
    //

    ArbiterEntry = NULL;
    if (BootAllocation->Length != 0) {
        ArbiterEntry = IopArbiterFindEntry(Arbiter,
                                           BootAllocation->Allocation,
                                           TRUE);
    }

    //
    // If there's something there, make sure it agrees.
    //

    if (ArbiterEntry != NULL) {

        //
        // If the entry isn't free, then it had better exactly work with the
        // entry there, and be shareable.
        //

        Conflict = FALSE;
        if (ArbiterEntry->Type != ArbiterSpaceFree) {
            if ((ArbiterEntry->Characteristics !=
                 Requirement->Characteristics) ||
                ((Requirement->Flags &
                  RESOURCE_FLAG_NOT_SHAREABLE) != 0) ||
                ((ArbiterEntry->Flags &
                  RESOURCE_FLAG_NOT_SHAREABLE) != 0) ||
                (Requirement->Length != ArbiterEntry->Length)) {

                Conflict = TRUE;
            }

            if ((ArbiterEntry->Allocation & (Requirement->Alignment - 1)) !=
                                                                       0) {

                Conflict = TRUE;
            }

            //
            // If different boot resources of the same device conflict with
            // each other, then assume the BIOS knows what it's doing there
            // and allow it.
            //

            if ((Conflict != FALSE) && (ArbiterEntry->Device == Device) &&
                ((ArbiterEntry->Flags & RESOURCE_FLAG_BOOT) != 0)) {

                Conflict = FALSE;
            }

            if (Conflict != FALSE) {
                Status = STATUS_RANGE_CONFLICT;
                goto ArbiterTryBootAllocationEnd;
            }
        }

    //
    // There is no entry, so add some free space and then allocate it. This
    // gives the BIOS the benefit of the doubt. For zero length
    // allocations, don't create free space, just insert.
    //

    } else if (BootAllocation->Length != 0) {
        Status = IopArbiterAddFreeSpace(Arbiter,
                                        BootAllocation->Allocation,
                                        BootAllocation->Length,
                                        0,
                                        NULL,
                                        0);

        if (!KSUCCESS(Status)) {
            goto ArbiterTryBootAllocationEnd;
        }

        ArbiterEntry = IopArbiterFindEntry(Arbiter,
                                           BootAllocation->Allocation,
                                           FALSE);

        ASSERT(ArbiterEntry != NULL);
    }

    //
    // Insert the boot allocation.
    //

    Status = IopArbiterInsertEntry(Arbiter,
                                   ArbiterSpaceReserved,
                                   Device,
                                   BootAllocation->Allocation,
                                   BootAllocation->Length,
                                   BootAllocation->Characteristics,
                                   Requirement->Flags | RESOURCE_FLAG_BOOT,
                                   Requirement,
                                   ArbiterEntry,
                                   &NewEntry);

    if (!KSUCCESS(Status)) {
        goto ArbiterTryBootAllocationEnd;
    }

    //
    // The space was successfully reserved, save it.
    //

    Context->Requirements[RequirementIndex].Allocation = NewEntry;
    Status = STATUS_SUCCESS;

ArbiterTryBootAllocationEnd:
    return Status;
}

PRESOURCE_ALLOCATION
IopArbiterFindBootAllocationForRequirement (
    PDEVICE Device,
    PRESOURCE_REQUIREMENT Requirement
    )

/*++

Routine Description:

    This routine attempts to find the boot resource allocation that matches
    with the given device's resource requirement.

Arguments:

    Device - Supplies a pointer to the device being queried.

    Requirement - Supplies a pointer to the resource requirement whose boot
        allocation counterpart should be found.

Return Value:

    Returns a pointer to the boot time allocation that satisfies the given
    requirement.

    NULL if the allocation could not be found.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    PRESOURCE_REQUIREMENT CurrentRequirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    ULONG ResourceIndex;

    //
    // Only the first requirement list is searched.
    //

    RequirementList =
            IoGetNextResourceConfiguration(Device->ResourceRequirements, NULL);

    ASSERT(RequirementList != NULL);

    //
    // Determine the index of the given requirement in the list of requirements.
    //

    ResourceIndex = 0;
    CurrentRequirement = IoGetNextResourceRequirement(RequirementList, NULL);
    while (CurrentRequirement != NULL) {
        if (CurrentRequirement == Requirement) {
            break;
        }

        ResourceIndex += 1;
        CurrentRequirement = IoGetNextResourceRequirement(RequirementList,
                                                          CurrentRequirement);
    }

    if (CurrentRequirement == NULL) {
        return NULL;
    }

    //
    // Now go that many entries into the boot allocation list.
    //

    AllocationList = Device->BootResources;
    if (AllocationList == NULL) {
        return NULL;
    }

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while ((ResourceIndex != 0) && (Allocation != NULL)) {
        ResourceIndex -= 1;
        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    if (Allocation == NULL) {
        return NULL;
    }

    //
    // Throw it out if the types don't match. Other checking is not done
    // because the allocation may satisfy an alternative instead of this exact
    // requirement.
    //

    if (Allocation->Type != Requirement->Type) {
        return NULL;
    }

    return Allocation;
}

VOID
IopArbiterClearContextAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine frees any reserved allocations made on behalf of the given
    allocation context.

Arguments:

    Context - Supplies a pointer to the context whose allocations should be
        cleared and freed.

Return Value:

    None.

--*/

{

    PRESOURCE_ARBITER Arbiter;
    PARBITER_ENTRY Entry;
    PARBITER_ALLOCATION_REQUIREMENT RequirementData;
    ULONG RequirementIndex;

    for (RequirementIndex = 0;
         RequirementIndex < Context->RequirementCount;
         RequirementIndex += 1) {

        RequirementData = &(Context->Requirements[RequirementIndex]);
        Arbiter = IOP_ARBITER_GET_ARBITER(Context, RequirementData);

        ASSERT(Arbiter != NULL);

        Entry = RequirementData->Allocation;
        if (Entry != NULL) {
            IopArbiterFreeEntry(Arbiter, Entry);
        }

        RequirementData->Allocation = NULL;
    }

    return;
}

VOID
IopArbiterLinkContextAllocations (
    PARBITER_ALLOCATION_CONTEXT Context
    )

/*++

Routine Description:

    This routine links each allocation made in an allocation context to its
    corresponding requirement and device.

Arguments:

    Context - Supplies a pointer to the context whose allocations should be
        cleared and freed.

Return Value:

    None.

--*/

{

    PDEVICE AllocationDevice;
    PARBITER_ENTRY Entry;
    ULONG RequirementIndex;

    for (RequirementIndex = 0;
         RequirementIndex < Context->RequirementCount;
         RequirementIndex += 1) {

        Entry = Context->Requirements[RequirementIndex].Allocation;
        if (Entry == NULL) {
            continue;
        }

        AllocationDevice = Entry->Device;
        INSERT_AFTER(&(Entry->ConfigurationListEntry),
                     &(AllocationDevice->ArbiterAllocationListHead));
    }

    return;
}

KSTATUS
IopDeferResourceAllocation (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine adds the given device to the array of devices whose resource
    allocation is being deferred until after all devices with boot allocations
    have been enumerated.

Arguments:

    Device - Supplies a pointer to the device to add.

Return Value:

    Status code.

--*/

{

    PDEVICE *NewArray;
    UINTN NewSize;

    NewSize = IoDelayedDeviceCount + 1;
    NewArray = MmAllocatePagedPool(NewSize * sizeof(PDEVICE),
                                   ARBITER_ALLOCATION_TAG);

    if (NewArray == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (IoDelayedDevices != NULL) {
        RtlCopyMemory(NewArray,
                      IoDelayedDevices,
                      (NewSize - 1) * sizeof(PDEVICE));

        MmFreePagedPool(IoDelayedDevices);
    }

    NewArray[NewSize - 1] = Device;
    IoDelayedDevices = NewArray;
    IoDelayedDeviceCount = NewSize;
    return STATUS_SUCCESS;
}


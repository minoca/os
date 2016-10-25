/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devres.c

Abstract:

    This module implements device resource requirement and allocation
    functionality.

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

//
// --------------------------------------------------------------------- Macros
//

//
// Some resources need to be non-paged because they may be used by the paging
// device during I/O transfers.
//

#define RESOURCE_TYPE_NON_PAGED(_ResourceType) \
    ((_ResourceType) == ResourceTypeDmaChannel)

//
// ---------------------------------------------------------------- Definitions
//

#define RESOURCE_ALLOCATION_TAG 0x4C736552 // 'LseR'

//
// Set a sane limit on how big these allocations can get.
//

#define RESOURCE_MAX_ADDITIONAL_DATA 0x1000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopResourceAllocationWorker (
    PVOID Parameter
    );

VOID
IopDelayedResourceAssignmentWorker (
    PVOID Parameter
    );

KSTATUS
IopCreateAndInitializeResourceRequirement (
    PRESOURCE_REQUIREMENT RequirementTemplate,
    PRESOURCE_REQUIREMENT *NewRequirement
    );

PSTR
IopGetResourceTypeString (
    RESOURCE_TYPE Type
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the array of devices that were delayed until the initial enumeration
// was complete.
//

PDEVICE *IoDelayedDevices;
UINTN IoDelayedDeviceCount;

//
// ------------------------------------------------------------------ Functions
//

//
// Resource requirement list support routines.
//

KERNEL_API
PRESOURCE_REQUIREMENT_LIST
IoCreateResourceRequirementList (
    VOID
    )

/*++

Routine Description:

    This routine creates a new empty resource requirement list.

Arguments:

    None.

Return Value:

    Returns a pointer to the new resource requirement list on success.

    NULL on allocation failure.

--*/

{

    PRESOURCE_REQUIREMENT_LIST List;

    List = MmAllocatePagedPool(sizeof(RESOURCE_REQUIREMENT_LIST),
                               RESOURCE_ALLOCATION_TAG);

    if (List == NULL) {
        return NULL;
    }

    RtlZeroMemory(List, sizeof(RESOURCE_REQUIREMENT_LIST));
    INITIALIZE_LIST_HEAD(&(List->RequirementListHead));
    return List;
}

KERNEL_API
VOID
IoDestroyResourceRequirementList (
    PRESOURCE_REQUIREMENT_LIST ResourceRequirementList
    )

/*++

Routine Description:

    This routine releases the memory associated with a resource requirement
    list, and any items on that list.

Arguments:

    ResourceRequirementList - Supplies a pointer to the resource requirement
        list to destroy.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PRESOURCE_REQUIREMENT ResourceRequirement;

    while (LIST_EMPTY(&(ResourceRequirementList->RequirementListHead)) ==
                                                                       FALSE) {

        CurrentEntry = ResourceRequirementList->RequirementListHead.Next;
        ResourceRequirement = LIST_VALUE(CurrentEntry,
                                         RESOURCE_REQUIREMENT,
                                         ListEntry);

        IoRemoveResourceRequirement(ResourceRequirement);
    }

    if (ResourceRequirementList->ListEntry.Next != NULL) {
        LIST_REMOVE(&(ResourceRequirementList->ListEntry));
    }

    MmFreePagedPool(ResourceRequirementList);
    return;
}

KERNEL_API
KSTATUS
IoCreateAndAddResourceRequirement (
    PRESOURCE_REQUIREMENT Requirement,
    PRESOURCE_REQUIREMENT_LIST ResourceRequirementList,
    PRESOURCE_REQUIREMENT *NewRequirement
    )

/*++

Routine Description:

    This routine creates a new resource requirement from the given template and
    inserts it into the given resource requirement list.

Arguments:

    Requirement - Supplies a pointer to the resource requirement to use as a
        template. The memory passed in will not actually be used, a copy of the
        requirement will be created, initialized, and placed on the list.

    ResourceRequirementList - Supplies a pointer to the resource requirement
        list to add the requirement to.

    NewRequirement - Supplies an optional pointer to the resource requirement
        that was created. The system owns this memory, the caller should not
        attempt to free it directly.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if parameter validation failed.

    STATUS_INSUFFICIENT_RESOURCES if the required memory could not be allocated.

--*/

{

    PRESOURCE_REQUIREMENT CreatedRequirement;
    KSTATUS Status;

    CreatedRequirement = NULL;
    Status = IopCreateAndInitializeResourceRequirement(Requirement,
                                                       &CreatedRequirement);

    if (!KSUCCESS(Status)) {
        goto CreateAndAddResourceRequirementEnd;
    }

    //
    // Add the requirement to the end of the list.
    //

    INSERT_BEFORE(&(CreatedRequirement->ListEntry),
                  &(ResourceRequirementList->RequirementListHead));

    Status = STATUS_SUCCESS;

CreateAndAddResourceRequirementEnd:
    if (NewRequirement != NULL) {
        *NewRequirement = CreatedRequirement;
    }

    return Status;
}

KERNEL_API
VOID
IoRemoveResourceRequirement (
    PRESOURCE_REQUIREMENT Requirement
    )

/*++

Routine Description:

    This routine removes the given resource descriptor from its resource list
    and frees the memory associated with that descriptor.

Arguments:

    Requirement - Supplies a pointer to the resource requirement to remove and
        destroy.

Return Value:

    None.

--*/

{

    PRESOURCE_REQUIREMENT Alternative;
    PLIST_ENTRY CurrentEntry;

    //
    // Loop through and destroy all alternatives to this resource requirement.
    //

    CurrentEntry = Requirement->AlternativeListEntry.Next;
    while (CurrentEntry != &(Requirement->AlternativeListEntry)) {
        Alternative = LIST_VALUE(CurrentEntry,
                                 RESOURCE_REQUIREMENT,
                                 AlternativeListEntry);

        //
        // It's important to move the list entry before the alternative is
        // destroyed!
        //

        CurrentEntry = CurrentEntry->Next;
        IoRemoveResourceRequirementAlternative(Alternative);
    }

    ASSERT(LIST_EMPTY(&(Requirement->AlternativeListEntry)) != FALSE);

    LIST_REMOVE(&(Requirement->ListEntry));
    MmFreePagedPool(Requirement);
    return;
}

KERNEL_API
KSTATUS
IoCreateAndAddResourceRequirementAlternative (
    PRESOURCE_REQUIREMENT Alternative,
    PRESOURCE_REQUIREMENT Requirement
    )

/*++

Routine Description:

    This routine creates a new resource requirement alternative from the given
    template and inserts it into the given resource requirement alternative
    list.

Arguments:

    Alternative - Supplies a pointer to the resource requirement to use as a
        template. The memory passed in will not actually be used, a copy of the
        requirement will be created, initialized, and placed on the list.

    Requirement - Supplies a pointer to the resource requirement to add the
        alternative to.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if parameter validation failed.

    STATUS_INSUFFICIENT_RESOURCES if the required memory could not be allocated.

--*/

{

    PRESOURCE_REQUIREMENT NewRequirement;
    KSTATUS Status;

    Status = IopCreateAndInitializeResourceRequirement(Alternative,
                                                       &NewRequirement);

    if (!KSUCCESS(Status)) {
        goto CreateAndAddResourceRequirementAlternativeEnd;
    }

    //
    // Add the alternative to the end of the list.
    //

    INSERT_BEFORE(&(NewRequirement->AlternativeListEntry),
                  &(Requirement->AlternativeListEntry));

    Status = STATUS_SUCCESS;

CreateAndAddResourceRequirementAlternativeEnd:
    return Status;
}

KERNEL_API
VOID
IoRemoveResourceRequirementAlternative (
    PRESOURCE_REQUIREMENT Alternative
    )

/*++

Routine Description:

    This routine removes the given resource requirement alternative from its
    resource list and frees the memory associated with that descriptor.

Arguments:

    Alternative - Supplies a pointer to the resource requirement alternative
        to remove and destroy.

Return Value:

    None.

--*/

{

    //
    // This had better be an alternative and not a first requirement.
    //

    ASSERT(Alternative->ListEntry.Next == NULL);

    LIST_REMOVE(&(Alternative->AlternativeListEntry));
    MmFreePagedPool(Alternative);
    return;
}

KERNEL_API
KSTATUS
IoCreateAndAddInterruptVectorsForLines (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList,
    PRESOURCE_REQUIREMENT VectorTemplate
    )

/*++

Routine Description:

    This routine creates a new vector resource requirement for each interrupt
    line requirement in the given configuration list.

Arguments:

    ConfigurationList - Supplies a pointer to the resource configuration list
        to iterate through.

    VectorTemplate - Supplies a pointer to a template to use when creating the
        vector resource requirements.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if parameter validation failed.

    STATUS_INSUFFICIENT_RESOURCES if the required memory could not be allocated.

--*/

{

    ULONGLONG LineCharacteristics;
    PRESOURCE_REQUIREMENT NextRequirement;
    PRESOURCE_REQUIREMENT Requirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    KSTATUS Status;
    ULONGLONG VectorCharacteristics;

    //
    // Loop through all configuration lists.
    //

    if (ConfigurationList == NULL) {
        Status = STATUS_SUCCESS;
        goto CreateAndAddInterruptVectorsForLinesEnd;
    }

    RequirementList = IoGetNextResourceConfiguration(ConfigurationList, NULL);
    while (RequirementList != NULL) {

        //
        // Loop through every requirement in the list.
        //

        Requirement = IoGetNextResourceRequirement(RequirementList, NULL);
        while (Requirement != NULL) {

            //
            // Get the next resource requirement.
            //

            NextRequirement = IoGetNextResourceRequirement(RequirementList,
                                                           Requirement);

            //
            // Skip the requirement if it is not an interrupt line.
            //

            if (Requirement->Type != ResourceTypeInterruptLine) {
                Requirement = NextRequirement;
                continue;
            }

            //
            // The requirement is an interrupt line. Add a vector requirement
            // based on the template.
            //

            VectorCharacteristics = 0;
            LineCharacteristics = Requirement->Characteristics;
            if ((LineCharacteristics & INTERRUPT_LINE_ACTIVE_LOW) != 0) {
                VectorCharacteristics |= INTERRUPT_VECTOR_ACTIVE_LOW;
            }

            if ((LineCharacteristics & INTERRUPT_LINE_ACTIVE_HIGH) != 0) {
                VectorCharacteristics |= INTERRUPT_VECTOR_ACTIVE_HIGH;
            }

            if ((LineCharacteristics & INTERRUPT_LINE_EDGE_TRIGGERED) != 0) {
                VectorCharacteristics |= INTERRUPT_VECTOR_EDGE_TRIGGERED;
            }

            //
            // Secondary interrupt lines have run-levels that may not
            // correspond in a direct way to their interrupt vector. These
            // types of vectors cannot be shared as it might create a conflict
            // of different run-levels for the same vector.
            //

            if ((LineCharacteristics & INTERRUPT_LINE_SECONDARY) != 0) {
                VectorTemplate->Flags |= RESOURCE_FLAG_NOT_SHAREABLE;
            }

            VectorTemplate->Characteristics = VectorCharacteristics;
            VectorTemplate->OwningRequirement = Requirement;
            Status = IoCreateAndAddResourceRequirement(VectorTemplate,
                                                       RequirementList,
                                                       NULL);

            if (!KSUCCESS(Status)) {
                goto CreateAndAddInterruptVectorsForLinesEnd;
            }

            Requirement = NextRequirement;
        }

        //
        // Get the next possible resource configuration.
        //

        RequirementList = IoGetNextResourceConfiguration(ConfigurationList,
                                                         RequirementList);
    }

    Status = STATUS_SUCCESS;

CreateAndAddInterruptVectorsForLinesEnd:
    return Status;
}

KERNEL_API
PRESOURCE_REQUIREMENT
IoGetNextResourceRequirement (
    PRESOURCE_REQUIREMENT_LIST ResourceRequirementList,
    PRESOURCE_REQUIREMENT CurrentEntry
    )

/*++

Routine Description:

    This routine returns a pointer to the next resource requirment in the
    resource requirement list.

Arguments:

    ResourceRequirementList - Supplies a pointer to the resource requirement
        list to iterate through.

    CurrentEntry - Supplies an optional pointer to the previous resource
        requirement. If supplied, the function will return the resource
        requirement immediately after this one in the list. If NULL is
        supplied, this routine will return the first resource requirement in the
        list.

Return Value:

    Returns a pointer to the next resource requirement in the given list on
    success, or NULL if the last resource requirement was reached.

--*/

{

    PLIST_ENTRY NextEntry;

    if (CurrentEntry != NULL) {
        NextEntry = CurrentEntry->ListEntry.Next;

    } else {
        NextEntry = ResourceRequirementList->RequirementListHead.Next;
    }

    if (NextEntry == &(ResourceRequirementList->RequirementListHead)) {
        return NULL;
    }

    return LIST_VALUE(NextEntry, RESOURCE_REQUIREMENT, ListEntry);
}

KERNEL_API
PRESOURCE_REQUIREMENT
IoGetNextResourceRequirementAlternative (
    PRESOURCE_REQUIREMENT ResourceRequirement,
    PRESOURCE_REQUIREMENT CurrentEntry
    )

/*++

Routine Description:

    This routine returns a pointer to the next resource requirment alternative
    in the alternative list for the requirement.

Arguments:

    ResourceRequirement - Supplies a pointer to the resource requirement at
        the head of the list.

    CurrentEntry - Supplies an optional pointer to the current requirement
        alternative.

Return Value:

    Returns a pointer to the next resource requirement alternative in the list
    on success, or NULL if the last resource requirement alternative was
    reached.

--*/

{

    PLIST_ENTRY NextEntry;

    if (CurrentEntry != NULL) {
        NextEntry = CurrentEntry->AlternativeListEntry.Next;

    } else {
        NextEntry = ResourceRequirement->AlternativeListEntry.Next;
    }

    if (NextEntry == &(ResourceRequirement->AlternativeListEntry)) {
        return NULL;
    }

    return LIST_VALUE(NextEntry, RESOURCE_REQUIREMENT, AlternativeListEntry);
}

//
// Resource configuration list routines.
//

KERNEL_API
PRESOURCE_CONFIGURATION_LIST
IoCreateResourceConfigurationList (
    PRESOURCE_REQUIREMENT_LIST FirstConfiguration
    )

/*++

Routine Description:

    This routine creates a new resource configuration list. A resource
    configuration list is a collection of resource requirement lists, arranged
    from most desirable to least desirable. The system attempts selects the
    most desirable resource configuration that can be afforded.

Arguments:

    FirstConfiguration - Supplies an optional pointer to the first configuration
        to put on the list. If NULL is supplied, an empty resource
        configuration list will be created.

Return Value:

    Returns a pointer to the new resource configuration list on success.

    NULL on allocation failure.

--*/

{

    PRESOURCE_CONFIGURATION_LIST List;

    List = MmAllocatePagedPool(sizeof(RESOURCE_CONFIGURATION_LIST),
                               RESOURCE_ALLOCATION_TAG);

    if (List == NULL) {
        return NULL;
    }

    RtlZeroMemory(List, sizeof(RESOURCE_CONFIGURATION_LIST));
    INITIALIZE_LIST_HEAD(&(List->RequirementListListHead));
    if (FirstConfiguration != NULL) {
        INSERT_AFTER(&(FirstConfiguration->ListEntry),
                     &(List->RequirementListListHead));
    }

    return List;
}

KERNEL_API
VOID
IoDestroyResourceConfigurationList (
    PRESOURCE_CONFIGURATION_LIST ResourceConfigurationList
    )

/*++

Routine Description:

    This routine releases the memory associated with a resource configuration
    list, and any resource requirement lists it may contain.

Arguments:

    ResourceConfigurationList - Supplies a pointer to the resource configuration
        list to destroy.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PRESOURCE_REQUIREMENT_LIST RequirementList;

    while (LIST_EMPTY(&(ResourceConfigurationList->RequirementListListHead)) ==
                                                                       FALSE) {

        CurrentEntry = ResourceConfigurationList->RequirementListListHead.Next;
        RequirementList = LIST_VALUE(CurrentEntry,
                                     RESOURCE_REQUIREMENT_LIST,
                                     ListEntry);

        IoDestroyResourceRequirementList(RequirementList);
    }

    MmFreePagedPool(ResourceConfigurationList);
    return;
}

KERNEL_API
KSTATUS
IoAddResourceConfiguration (
    PRESOURCE_REQUIREMENT_LIST Configuration,
    PRESOURCE_REQUIREMENT_LIST ConfigurationToInsertAfter,
    PRESOURCE_CONFIGURATION_LIST ConfigurationList
    )

/*++

Routine Description:

    This routine inserts an initialized resource configuration into a
    configuration list.

Arguments:

    Configuration - Supplies a pointer to the resource configuration to insert
        into the list.

    ConfigurationToInsertAfter - Supplies an optional pointer indicating the
        location in the list to insert the configuration. If this pointer is
        supplied, the configuration will be inserted immediate after this
        parameter in the list. If NULL is supplied, the configuration will be
        added to the end of the list.

    ConfigurationList - Supplies a pointer to the list to add the configuration
        to.

Return Value:

    Status code.

--*/

{

    ASSERT(Configuration->ListEntry.Next == NULL);

    if (ConfigurationToInsertAfter != NULL) {
        INSERT_AFTER(&(Configuration->ListEntry),
                     &(ConfigurationToInsertAfter->ListEntry));

    } else {
        INSERT_BEFORE(&(Configuration->ListEntry),
                      &(ConfigurationList->RequirementListListHead));
    }

    return STATUS_SUCCESS;
}

KERNEL_API
VOID
IoRemoveResourceConfiguration (
    PRESOURCE_REQUIREMENT_LIST Configuration,
    PRESOURCE_CONFIGURATION_LIST ConfigurationList
    )

/*++

Routine Description:

    This routine removes the given resource descriptor from its resource list.
    It does not free the memory associated with the configuration.

Arguments:

    Configuration - Supplies a pointer to the configuration to remove.

    ConfigurationList - Supplies a pointer to the configuration list to remove
        the configuration from.

Return Value:

    None.

--*/

{

    ASSERT(Configuration->ListEntry.Next != NULL);

    LIST_REMOVE(&(Configuration->ListEntry));
    Configuration->ListEntry.Next = NULL;
    return;
}

KERNEL_API
PRESOURCE_REQUIREMENT_LIST
IoGetNextResourceConfiguration (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList,
    PRESOURCE_REQUIREMENT_LIST CurrentEntry
    )

/*++

Routine Description:

    This routine returns a pointer to the next resource configuration in the
    resource configuration list.

Arguments:

    ConfigurationList - Supplies a pointer to the resource configuration list
        to iterate through.

    CurrentEntry - Supplies an optional pointer to the previous resource
        requirement list. If supplied, the function will return the
        configuration immediately after this one in the list. If NULL is
        supplied, this routine will return the first configuration in the
        list.

Return Value:

    Returns a pointer to the next resource configuration in the given list on
    success, or NULL if the last resource requirement was reached.

--*/

{

    PLIST_ENTRY NextEntry;

    if (CurrentEntry != NULL) {
        NextEntry = CurrentEntry->ListEntry.Next;

    } else {
        NextEntry = ConfigurationList->RequirementListListHead.Next;
    }

    if (NextEntry == &(ConfigurationList->RequirementListListHead)) {
        return NULL;
    }

    return LIST_VALUE(NextEntry, RESOURCE_REQUIREMENT_LIST, ListEntry);
}

//
// Resource allocation list support routines.
//

KERNEL_API
PRESOURCE_ALLOCATION_LIST
IoCreateResourceAllocationList (
    VOID
    )

/*++

Routine Description:

    This routine creates a new empty resource allocation list.

Arguments:

    None.

Return Value:

    Returns a pointer to the new resource allocation list on success.

    NULL on allocation failure.

--*/

{

    PRESOURCE_ALLOCATION_LIST List;

    List = MmAllocatePagedPool(sizeof(PRESOURCE_ALLOCATION_LIST),
                               RESOURCE_ALLOCATION_TAG);

    if (List == NULL) {
        return NULL;
    }

    RtlZeroMemory(List, sizeof(PRESOURCE_ALLOCATION_LIST));
    INITIALIZE_LIST_HEAD(&(List->AllocationListHead));
    return List;
}

KERNEL_API
VOID
IoDestroyResourceAllocationList (
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList
    )

/*++

Routine Description:

    This routine releases the memory associated with a resource allocation
    list, and any items on that list.

Arguments:

    ResourceAllocationList - Supplies a pointer to the resource allocation
        list to destroy.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PRESOURCE_ALLOCATION ResourceAllocation;

    while (LIST_EMPTY(&(ResourceAllocationList->AllocationListHead)) ==
                                                                       FALSE) {

        CurrentEntry = ResourceAllocationList->AllocationListHead.Next;
        ResourceAllocation = LIST_VALUE(CurrentEntry,
                                        RESOURCE_ALLOCATION,
                                        ListEntry);

        IoRemoveResourceAllocation(ResourceAllocation, ResourceAllocationList);
    }

    MmFreePagedPool(ResourceAllocationList);
    return;
}

KERNEL_API
KSTATUS
IoCreateAndAddResourceAllocation (
    PRESOURCE_ALLOCATION Allocation,
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList
    )

/*++

Routine Description:

    This routine creates a new resource allocation from the given template and
    inserts it into the given resource allocation list.

Arguments:

    Allocation - Supplies a pointer to the resource allocation to use as a
        template. The memory passed in will not actually be used, a copy of the
        allocation will be created, initialized, and placed on the list.

    ResourceAllocationList - Supplies a pointer to the resource allocation
        list to add the allocation to.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if parameter validation failed.

    STATUS_INSUFFICIENT_RESOURCES if the required memory could not be allocated.

--*/

{

    UINTN AllocationSize;
    UINTN DataSize;
    PRESOURCE_ALLOCATION NewAllocation;
    KSTATUS Status;

    //
    // Check parameters.
    //

    if ((Allocation->Type == ResourceTypeInvalid) ||
        (Allocation->Type >= ResourceTypeCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreateAndAddResourceAllocationEnd;
    }

    DataSize = Allocation->DataSize;
    if (DataSize > RESOURCE_MAX_ADDITIONAL_DATA) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateAndAddResourceAllocationEnd;
    }

    //
    // Create the new resource allocation.
    //

    AllocationSize = sizeof(RESOURCE_ALLOCATION) + DataSize;
    if (RESOURCE_TYPE_NON_PAGED(Allocation->Type)) {
        NewAllocation = MmAllocateNonPagedPool(AllocationSize,
                                               RESOURCE_ALLOCATION_TAG);

    } else {
        NewAllocation = MmAllocatePagedPool(AllocationSize,
                                            RESOURCE_ALLOCATION_TAG);
    }

    if (NewAllocation == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAndAddResourceAllocationEnd;
    }

    RtlZeroMemory(NewAllocation, sizeof(RESOURCE_ALLOCATION));
    NewAllocation->Type = Allocation->Type;
    NewAllocation->Allocation = Allocation->Allocation;
    NewAllocation->Length = Allocation->Length;
    NewAllocation->Characteristics = Allocation->Characteristics;
    NewAllocation->Flags = Allocation->Flags;
    NewAllocation->OwningAllocation = Allocation->OwningAllocation;
    NewAllocation->Provider = Allocation->Provider;
    if (DataSize != 0) {
        NewAllocation->Data = NewAllocation + 1;
        NewAllocation->DataSize = DataSize;
        RtlCopyMemory(NewAllocation->Data, Allocation->Data, DataSize);
    }

    //
    // Add the allocation to the end of the list.
    //

    INSERT_BEFORE(&(NewAllocation->ListEntry),
                  &(ResourceAllocationList->AllocationListHead));

    Status = STATUS_SUCCESS;

CreateAndAddResourceAllocationEnd:
    return Status;
}

KERNEL_API
VOID
IoRemoveResourceAllocation (
    PRESOURCE_ALLOCATION Allocation,
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList
    )

/*++

Routine Description:

    This routine removes the given resource descriptor from its resource list
    and frees the memory associated with that descriptor.

Arguments:

    Allocation - Supplies a pointer to the allocation to remove.

    ResourceAllocationList - Supplies a pointer to the list to remove the
        allocation from.

Return Value:

    None.

--*/

{

    ASSERT(ResourceAllocationList != NULL);

    LIST_REMOVE(&(Allocation->ListEntry));
    if (RESOURCE_TYPE_NON_PAGED(Allocation->Type)) {
        MmFreeNonPagedPool(Allocation);

    } else {
        MmFreePagedPool(Allocation);
    }

    return;
}

KERNEL_API
PRESOURCE_ALLOCATION
IoGetNextResourceAllocation (
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList,
    PRESOURCE_ALLOCATION CurrentEntry
    )

/*++

Routine Description:

    This routine returns a pointer to the next resource allocation in the
    resource allocation list.

Arguments:

    ResourceAllocationList - Supplies a pointer to the resource allocation
        list to iterate through.

    CurrentEntry - Supplies an optional pointer to the previous resource
        allocation. If supplied, the function will return the resource
        allocation immediately after this one in the list. If NULL is
        supplied, this routine will return the first resource allocation in the
        list.

Return Value:

    Returns a pointer to the next resource allocation in the given list on
    success, or NULL if the last resource allocation was reached.

--*/

{

    PLIST_ENTRY NextEntry;

    if (ResourceAllocationList == NULL) {
        return NULL;
    }

    if (CurrentEntry != NULL) {
        NextEntry = CurrentEntry->ListEntry.Next;

    } else {
        NextEntry = ResourceAllocationList->AllocationListHead.Next;
    }

    if (NextEntry == &(ResourceAllocationList->AllocationListHead)) {
        return NULL;
    }

    return LIST_VALUE(NextEntry, RESOURCE_ALLOCATION, ListEntry);
}

KERNEL_API
VOID
IoDebugPrintResourceConfigurationList (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList
    )

/*++

Routine Description:

    This routine prints a resource configuration list out to the debugger.

Arguments:

    ConfigurationList - Supplies a pointer to the resource configuration list to
        print.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PRESOURCE_REQUIREMENT_LIST RequirementList;

    RtlDebugPrint("Resource Configuration List at 0x%x:\n", ConfigurationList);
    CurrentEntry = ConfigurationList->RequirementListListHead.Next;
    while (CurrentEntry != &(ConfigurationList->RequirementListListHead)) {
        RequirementList = LIST_VALUE(CurrentEntry,
                                     RESOURCE_REQUIREMENT_LIST,
                                     ListEntry);

        CurrentEntry = CurrentEntry->Next;
        IoDebugPrintResourceRequirementList(1, RequirementList);
    }

    return;
}

KERNEL_API
VOID
IoDebugPrintResourceRequirementList (
    ULONG IndentationLevel,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    )

/*++

Routine Description:

    This routine prints a resource requirement list out to the debugger.

Arguments:

    IndentationLevel - Supplies the indentation level to print this list
        at. Supply 0 if this function is called directly.

    RequirementList - Supplies a pointer to the resource requirement list to
        print.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONG IndentationIndex;
    PRESOURCE_REQUIREMENT Requirement;

    for (IndentationIndex = 0;
         IndentationIndex < IndentationLevel;
         IndentationIndex += 1) {

        RtlDebugPrint("  ");
    }

    RtlDebugPrint("Resource Requirement List at 0x%x:\n", RequirementList);
    CurrentEntry = RequirementList->RequirementListHead.Next;
    while (CurrentEntry != &(RequirementList->RequirementListHead)) {
        Requirement = LIST_VALUE(CurrentEntry, RESOURCE_REQUIREMENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        IoDebugPrintResourceRequirement(IndentationLevel + 1, Requirement);
    }

    return;
}

KERNEL_API
VOID
IoDebugPrintResourceRequirement (
    ULONG IndentationLevel,
    PRESOURCE_REQUIREMENT Requirement
    )

/*++

Routine Description:

    This routine prints a resource requirement out to the debugger.

Arguments:

    IndentationLevel - Supplies the indentation level to print this requirement
        at. Supply 0 if this function is called directly.

    Requirement - Supplies a pointer to the resource requirement to print.

Return Value:

    None.

--*/

{

    PRESOURCE_REQUIREMENT Alternative;
    PLIST_ENTRY CurrentEntry;
    ULONG IndentationIndex;
    PSTR ResourceType;

    for (IndentationIndex = 0;
         IndentationIndex < IndentationLevel;
         IndentationIndex += 1) {

        RtlDebugPrint("  ");
    }

    //
    // Get the resource type.
    //

    ResourceType = IopGetResourceTypeString(Requirement->Type);
    RtlDebugPrint("0x%x %16s: From 0x%08I64x to 0x%08I64x, Len 0x%I64x, "
                  "Align 0x%08I64x, Char: 0x%I64x, Flags: 0x%I64x, "
                  "Owner: 0x%08x\n",
                  Requirement,
                  ResourceType,
                  Requirement->Minimum,
                  Requirement->Maximum,
                  Requirement->Length,
                  Requirement->Alignment,
                  Requirement->Characteristics,
                  Requirement->Flags,
                  Requirement->OwningRequirement);

    //
    // If the requirement is not attached to a resource requirement list,
    // don't try to traverse alternatives.
    //

    if (Requirement->ListEntry.Next == NULL) {
        return;
    }

    //
    // Loop through and recursively print out all alternatives.
    //

    CurrentEntry = Requirement->AlternativeListEntry.Next;
    while (CurrentEntry != &(Requirement->AlternativeListEntry)) {
        Alternative = LIST_VALUE(CurrentEntry,
                                 RESOURCE_REQUIREMENT,
                                 AlternativeListEntry);

        CurrentEntry = CurrentEntry->Next;
        IoDebugPrintResourceRequirement(IndentationLevel + 1, Alternative);
    }

    return;
}

KERNEL_API
VOID
IoDebugPrintResourceAllocationList (
    ULONG IndentationLevel,
    PRESOURCE_ALLOCATION_LIST AllocationList
    )

/*++

Routine Description:

    This routine prints a resource allocation list out to the debugger.

Arguments:

    IndentationLevel - Supplies the indentation level to print this list
        at. Supply 0 if this function is called directly.

    AllocationList - Supplies a pointer to the resource allocation list to
        print.

Return Value:

    None.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PLIST_ENTRY CurrentEntry;
    ULONG IndentationIndex;

    for (IndentationIndex = 0;
         IndentationIndex < IndentationLevel;
         IndentationIndex += 1) {

        RtlDebugPrint("  ");
    }

    RtlDebugPrint("Resource Allocation List at 0x%x:\n", AllocationList);
    CurrentEntry = AllocationList->AllocationListHead.Next;
    while (CurrentEntry != &(AllocationList->AllocationListHead)) {
        Allocation = LIST_VALUE(CurrentEntry, RESOURCE_ALLOCATION, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        IoDebugPrintResourceAllocation(IndentationLevel + 1, Allocation);
    }

    return;
}

KERNEL_API
VOID
IoDebugPrintResourceAllocation (
    ULONG IndentationLevel,
    PRESOURCE_ALLOCATION Allocation
    )

/*++

Routine Description:

    This routine prints a resource allocation out to the debugger.

Arguments:

    IndentationLevel - Supplies the indentation level to print this allocation
        at. Supply 0 if this function is called directly.

    Allocation - Supplies a pointer to the resource allocation to print.

Return Value:

    None.

--*/

{

    ULONG IndentationIndex;
    PSTR ResourceType;

    for (IndentationIndex = 0;
         IndentationIndex < IndentationLevel;
         IndentationIndex += 1) {

        RtlDebugPrint("  ");
    }

    ResourceType = IopGetResourceTypeString(Allocation->Type);
    RtlDebugPrint("0x%08x %16s: 0x%08I64x, Len 0x%08I64x, Char 0x%I64x, "
                  "Flags 0x%I64x Owner 0x%08x ",
                  Allocation,
                  ResourceType,
                  Allocation->Allocation,
                  Allocation->Length,
                  Allocation->Characteristics,
                  Allocation->Flags,
                  Allocation->OwningAllocation);

    if ((Allocation->Flags & RESOURCE_FLAG_NOT_SHAREABLE) != 0) {
        RtlDebugPrint("NotShared ");
    }

    RtlDebugPrint("\n");
    return;
}

KSTATUS
IopQueueResourceAssignment (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine puts this device in the resource assignment queue.

Arguments:

    Device - Supplies a pointer to the device to queue for resource assignment.

Return Value:

    Status code indicating whether or not the device was successfully queued.
    (Not that it successfully made it through the queue or was processed in
    any way).

--*/

{

    DEVICE_STATE OldState;
    KSTATUS Status;

    //
    // If the device has no resource requirements and no boot resources, move
    // the device straight to resources assigned.
    //

    if ((Device->ResourceRequirements == NULL) &&
        (Device->BootResources == NULL)) {

        Status = STATUS_SUCCESS;
        IopSetDeviceState(Device, DeviceResourcesAssigned);
        goto QueueResourceAssignmentEnd;
    }

    //
    // Set the state as if the operation was successful so that this routine is
    // not racing with the worker to set the state later.
    //

    OldState = Device->State;
    IopSetDeviceState(Device, DeviceResourceAssignmentQueued);
    RtlAtomicAdd(&IoDeviceWorkItemsQueued, 1);
    Status = KeCreateAndQueueWorkItem(IoResourceAllocationWorkQueue,
                                      WorkPriorityNormal,
                                      IopResourceAllocationWorker,
                                      Device);

    //
    // If it didn't work, set the state back to what it was before, resource
    // assignment work was not queued.
    //

    if (!KSUCCESS(Status)) {
        RtlAtomicAdd(&IoDeviceWorkItemsQueued, -1);
        IopSetDeviceState(Device, OldState);
        IopSetDeviceProblem(Device,
                            DeviceProblemFailedToQueueResourceAssignmentWork,
                            Status);
    }

QueueResourceAssignmentEnd:
    return Status;
}

KSTATUS
IopQueueDelayedResourceAssignment (
    VOID
    )

/*++

Routine Description:

    This routine queues resource assignment for devices that were delayed to
    allow devices with boot resources to go first.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = KeCreateAndQueueWorkItem(IoResourceAllocationWorkQueue,
                                      WorkPriorityNormal,
                                      IopDelayedResourceAssignmentWorker,
                                      NULL);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
IopResourceAllocationWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is workhorse function fo the resource allocation work queue.
    It attempts to satisfy the resource requirements of a device.

Arguments:

    Parameter - Supplies the work item parameter, which in this case is a
        pointer to the device to try and allocate resource for.

Return Value:

    None. If the allocation is successful, the state in the device will be
    advanced and work will be queued to start the device. If the allocation
    fails, the device will be marked with a problem code.

--*/

{

    PDEVICE Device;
    UINTN OldWorkItemCount;
    KSTATUS Status;

    Device = (PDEVICE)Parameter;

    //
    // Attempt to satisfy the resource requirements of the device.
    //

    Status = IopProcessResourceRequirements(Device);
    if (!KSUCCESS(Status)) {
        IopSetDeviceProblem(Device, DeviceProblemResourceConflict, Status);
        goto ResourceAllocationWorkerEnd;
    }

    //
    // Resources were successfully allocated. Advance the state and kick the
    // device to start.
    //

    IopSetDeviceState(Device, DeviceResourcesAssigned);
    Status = IopQueueDeviceWork(Device, DeviceActionStart, NULL, 0);
    if (!KSUCCESS(Status)) {
        IopSetDeviceProblem(Device, DeviceProblemFailedToQueueStart, Status);
    }

ResourceAllocationWorkerEnd:
    OldWorkItemCount = RtlAtomicAdd(&IoDeviceWorkItemsQueued, -1);
    if (OldWorkItemCount == 1) {
        IopQueueDelayedResourceAssignment();
    }

    return;
}

VOID
IopDelayedResourceAssignmentWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine attempts to assign resources for all devices whose resource
    assignment was delayed to allow boot allocations to take priority.

Arguments:

    Parameter - Supplies the work item parameter, which in this case is unused.

Return Value:

    None.

--*/

{

    UINTN DeviceCount;
    UINTN DeviceIndex;
    PDEVICE *Devices;

    DeviceCount = IoDelayedDeviceCount;
    Devices = IoDelayedDevices;
    IoDelayedDevices = NULL;
    IoDelayedDeviceCount = 0;
    for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
        IopResourceAllocationWorker(Devices[DeviceIndex]);
    }

    if (DeviceCount != 0) {
        MmFreePagedPool(Devices);
    }

    return;
}

KSTATUS
IopCreateAndInitializeResourceRequirement (
    PRESOURCE_REQUIREMENT RequirementTemplate,
    PRESOURCE_REQUIREMENT *NewRequirement
    )

/*++

Routine Description:

    This routine creates a new resource requirement from the given template.

Arguments:

    RequirementTemplate - Supplies a pointer to the resource requirement to use
        as a template. The memory passed in will not actually be used, a copy of
        the requirement will be created and initialized. A copy of the
        additional data is also made.

    NewRequirement - Supplies a pointer where the new resource requirement will
        be returned upon success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if parameter validation failed.

    STATUS_INSUFFICIENT_RESOURCES if the required memory could not be allocated.

--*/

{

    UINTN AllocationSize;
    PRESOURCE_REQUIREMENT CreatedRequirement;
    UINTN DataSize;
    KSTATUS Status;

    CreatedRequirement = NULL;

    //
    // Check parameters.
    //

    if ((RequirementTemplate->Minimum != 0) &&
        ((RequirementTemplate->Minimum > RequirementTemplate->Maximum) ||
         (RequirementTemplate->Minimum + RequirementTemplate->Length >
          RequirementTemplate->Maximum) ||
         (RequirementTemplate->Minimum + RequirementTemplate->Length <
          RequirementTemplate->Minimum))) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreateAndInitializeResourceRequirementEnd;
    }

    if ((RequirementTemplate->Type == ResourceTypeInvalid) ||
        (RequirementTemplate->Type >= ResourceTypeCount)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreateAndInitializeResourceRequirementEnd;
    }

    DataSize = RequirementTemplate->DataSize;
    if (DataSize > RESOURCE_MAX_ADDITIONAL_DATA) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateAndInitializeResourceRequirementEnd;
    }

    AllocationSize = sizeof(RESOURCE_REQUIREMENT) + DataSize;

    //
    // Create the new requirement.
    //

    CreatedRequirement = MmAllocatePagedPool(AllocationSize,
                                             RESOURCE_ALLOCATION_TAG);

    if (CreatedRequirement == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAndInitializeResourceRequirementEnd;
    }

    RtlZeroMemory(CreatedRequirement, sizeof(RESOURCE_REQUIREMENT));
    INITIALIZE_LIST_HEAD(&(CreatedRequirement->AlternativeListEntry));
    CreatedRequirement->Type = RequirementTemplate->Type;
    CreatedRequirement->Minimum = RequirementTemplate->Minimum;
    CreatedRequirement->Maximum = RequirementTemplate->Maximum;
    CreatedRequirement->Alignment = RequirementTemplate->Alignment;
    if (CreatedRequirement->Alignment == 0) {
        CreatedRequirement->Alignment = 1;
    }

    CreatedRequirement->Length = RequirementTemplate->Length;
    CreatedRequirement->Characteristics = RequirementTemplate->Characteristics;
    CreatedRequirement->Flags = RequirementTemplate->Flags;
    CreatedRequirement->OwningRequirement =
                                        RequirementTemplate->OwningRequirement;

    CreatedRequirement->Provider = RequirementTemplate->Provider;
    if (DataSize != 0) {
        CreatedRequirement->Data = CreatedRequirement + 1;
        RtlCopyMemory(CreatedRequirement->Data,
                      RequirementTemplate->Data,
                      DataSize);

        CreatedRequirement->DataSize = DataSize;
    }

    Status = STATUS_SUCCESS;

CreateAndInitializeResourceRequirementEnd:
    if (!KSUCCESS(Status)) {
        if (CreatedRequirement != NULL) {
            MmFreePagedPool(CreatedRequirement);
            CreatedRequirement = NULL;
        }
    }

    *NewRequirement = CreatedRequirement;
    return Status;
}

PSTR
IopGetResourceTypeString (
    RESOURCE_TYPE Type
    )

/*++

Routine Description:

    This routine returns a string representing the given resource type.

Arguments:

    Type - Supplies the resource type.

Return Value:

    Returns a pointer to a constant read-only string representing the given
    resource type.

--*/

{

    PSTR ResourceType;

    switch (Type) {
    case ResourceTypeInvalid:
        ResourceType = "Invalid";
        break;

    case ResourceTypePhysicalAddressSpace:
        ResourceType = "Physical Address";
        break;

    case ResourceTypeIoPort:
        ResourceType = "I/O Port";
        break;

    case ResourceTypeInterruptLine:
        ResourceType = "Interrupt Line";
        break;

    case ResourceTypeInterruptVector:
        ResourceType = "Interrupt Vector";
        break;

    case ResourceTypeBusNumber:
        ResourceType = "Bus Number";
        break;

    case ResourceTypeDmaChannel:
        ResourceType = "DMA Channel";
        break;

    case ResourceTypeVendorSpecific:
        ResourceType = "Vendor Specific";
        break;

    case ResourceTypeGpio:
        ResourceType = "GPIO";
        break;

    case ResourceTypeSimpleBus:
        ResourceType = "SPB";
        break;

    default:
        ResourceType = "INVALID RESOURCE TYPE";
        break;
    }

    return ResourceType;
}


/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    arb.h

Abstract:

    This header contains internal definitions for the resource arbiters.

Author:

    Evan Green 12-Dec-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define ARBITER_ALLOCATION_TAG 0x21627241 // '!brA'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ARBITER_TYPE {
    ArbiterTypeInvalid = ResourceTypeInvalid,
    ArbiterTypePhysicalAddressSpace = ResourceTypePhysicalAddressSpace,
    ArbiterTypeIoPort = ResourceTypeIoPort,
    ArbiterTypeInterruptLine = ResourceTypeInterruptLine,
    ArbiterTypeInterruptVector = ResourceTypeInterruptVector,
    ArbiterTypeBusNumber = ResourceTypeBusNumber,
    ArbiterTypeVendorSpecific = ResourceTypeVendorSpecific,
    ArbiterTypeCount
} ARBITER_TYPE, *PARBITER_TYPE;

typedef enum _ARBITER_SPACE_TYPE {
    ArbiterSpaceInvalid,
    ArbiterSpaceFree,
    ArbiterSpaceReserved,
    ArbiterSpaceAllocated
} ARBITER_SPACE_TYPE, *PARBITER_SPACE_TYPE;

/*++

Structure Description:

    This structure defines an entry in the resource arbiter.

Members:

    ListEntry - Stores pointers to the next and previous arbiter entries in the
        arbiter.

    ConfigurationListEntry - Stores pointers to the next and previous arbiter
        allocations in the potential resource configuration. This allows all
        resources allocated to a device to be chained together. In all
        likelihood these will point to allocations in different arbiters.

    Type - Stores the nature of the allocation (free, occupied, etc.)

    Device - Stores a pointer to the device that this entry was allocated to.

    CorrespondingRequirement - Stores a pointer to the root requirement that is
        utiliizing this resource.

    Allocation - Stores the starting value of the allocation.

    Length - Stores the length of the allocation.

    Characteristics - Stores the characteristics of the allocation.

    FreeCharacteristics - Stores the characteristics of the region when it was
        free.

    Flags - Stores a bitfield about the allocation. See ARBITER_ENTRY_FLAG_*
        definitions.

    SourceAllocation - Stores an optional pointer to the resource that this
        allocation is derived from.

    TranslationOffset - Stores the offset that must be added to this allocation
        to get an allocation in the source allocation space.

    DependentEntry - Stores a pointer to an arbiter entry that is dependent on
        this entry in some way. For example, an interrupt vector arbiter entry
        may be dependent on an interrupt line arbiter entry because the same
        line cannot be allocated to more than one vector. Once a line arbiter
        allocation is made, the vector allocation depends on the result of the
        line entry.

--*/

typedef struct _ARBITER_ENTRY ARBITER_ENTRY, *PARBITER_ENTRY;
struct _ARBITER_ENTRY {
    LIST_ENTRY ListEntry;
    LIST_ENTRY ConfigurationListEntry;
    ARBITER_SPACE_TYPE Type;
    PDEVICE Device;
    PRESOURCE_REQUIREMENT CorrespondingRequirement;
    ULONGLONG Allocation;
    ULONGLONG Length;
    ULONGLONG Characteristics;
    ULONGLONG FreeCharacteristics;
    ULONG Flags;
    PRESOURCE_ALLOCATION SourceAllocation;
    ULONGLONG TranslationOffset;
    PARBITER_ENTRY DependentEntry;
};

/*++

Structure Description:

    This structure defines a resource arbiter.

Members:

    ListEntry - Stores pointers to the next and previous arbiters in the
        device's arbiter list.

    OwningDevice - Stores a pointer to the device that manages this arbiter.

    ResourceType - Stores the type of resource that this arbiter manages.

    Flags - Stores a bitmask of flags about this arbiter. See ARBITER_FLAG_*
        definitions.

    EntryListHead - Stores the head of the arbiter entry list.

--*/

typedef struct _RESOURCE_ARBITER {
    LIST_ENTRY ListEntry;
    PDEVICE OwningDevice;
    RESOURCE_TYPE ResourceType;
    ULONG Flags;
    LIST_ENTRY EntryListHead;
} RESOURCE_ARBITER, *PRESOURCE_ARBITER;

/*++

Structure Description:

    This structure defines an arbiter allocation context, a scratchpad of
    state used when trying to satisfy allocations of one or more devices.

Members:

    Arbiter - Stores an array of pointers to an arbiter of each resource type.
        These represent the arbiters being allocated from for this set of
        allocations.

    AmountNotAllocated - Stores an array of the total amount of space not
        successfully allocated in each arbiter. This array is used to determine
        which arbiter is under the most pressure.

    Device - Stores an array of pointers to devices. These represent the
        devices involved in this set of allocations.

    CurrentDeviceConfiguration - Supplies an array of pointers to device
        configurations. For each device, this represents which of the
        possible configurations is being worked on.

    DeviceCount - Stores the number of elements in the device and current
        configuration arrays.

    Requirement - Stores an array of pointers to all the requirements that have
        to be satisfied in this set.

    RequirementDevice - Stores an array of pointers to devices that own each
        of the requirements in the requirement array.

    Allocation - Stores an array of pointers to arbiter allocations that
        satisfy each of the requirements (the order corresponds one to one).

    RequirementCount - Stores the number of elements in the requirement and
        requirement device arrays.

--*/

typedef struct _ARBITER_ALLOCATION_CONTEXT {
    PRESOURCE_ARBITER Arbiter[ArbiterTypeCount];
    ULONGLONG AmountNotAllocated[ArbiterTypeCount];
    PDEVICE *Device;
    PRESOURCE_REQUIREMENT_LIST *CurrentDeviceConfiguration;
    ULONG DeviceCount;
    PRESOURCE_REQUIREMENT *Requirement;
    PDEVICE *RequirementDevice;
    PARBITER_ENTRY *Allocation;
    ULONG RequirementCount;
} ARBITER_ALLOCATION_CONTEXT, *PARBITER_ALLOCATION_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

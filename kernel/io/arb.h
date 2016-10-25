/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// --------------------------------------------------------------------- Macros
//

//
// The arbiters themselves are doubly indirected because a deduplicated
// array of arbiters is needed.
//

#define IOP_GET_ARBITER_DATA(_Context, _RequirementData) \
    (&((_Context)->ArbiterData[(_RequirementData)->ArbiterIndex]))

#define IOP_ARBITER_GET_ARBITER(_Context, _RequirementData) \
    (IOP_GET_ARBITER_DATA(_Context, _RequirementData)->Arbiter)

#define IOP_ARBITER_GET_DEVICE(_Context, _RequirementData) \
    ((_Context)->Device[(_RequirementData)->DeviceIndex])

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
    ArbiterTypeGpio = ResourceTypeGpio,
    ArbiterTypeSimpleBus = ResourceTypeSimpleBus,
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

    Flags - Stores a bitfield about the allocation. See ARBITER_ENTRY_FLAG_*
        definitions.

    Device - Stores a pointer to the device that this entry was allocated to.

    CorrespondingRequirement - Stores a pointer to the root requirement that is
        utiliizing this resource.

    SourceAllocation - Stores an optional pointer to the resource that this
        allocation is derived from.

    DependentEntry - Stores a pointer to an arbiter entry that is dependent on
        this entry in some way. For example, an interrupt vector arbiter entry
        may be dependent on an interrupt line arbiter entry because the same
        line cannot be allocated to more than one vector. Once a line arbiter
        allocation is made, the vector allocation depends on the result of the
        line entry.

    Allocation - Stores the starting value of the allocation.

    Length - Stores the length of the allocation.

    Characteristics - Stores the characteristics of the allocation.

    FreeCharacteristics - Stores the characteristics of the region when it was
        free.

    TranslationOffset - Stores the offset that must be added to this allocation
        to get an allocation in the source allocation space.

--*/

typedef struct _ARBITER_ENTRY ARBITER_ENTRY, *PARBITER_ENTRY;
struct _ARBITER_ENTRY {
    LIST_ENTRY ListEntry;
    LIST_ENTRY ConfigurationListEntry;
    ARBITER_SPACE_TYPE Type;
    ULONG Flags;
    PDEVICE Device;
    PRESOURCE_REQUIREMENT CorrespondingRequirement;
    PRESOURCE_ALLOCATION SourceAllocation;
    PARBITER_ENTRY DependentEntry;
    ULONGLONG Allocation;
    ULONGLONG Length;
    ULONGLONG Characteristics;
    ULONGLONG FreeCharacteristics;
    ULONGLONG TranslationOffset;
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

    This structure defines the data associated with an arbiter during an
    allocation proceeding.

Members:

    Arbiter - Stores a pointer to the arbiter itself.

    AmountNotAllocated - Stores the amount that could not be allocated from
        this arbiter during an allocation.

--*/

typedef struct _ARBITER_ALLOCATION_ARBITER_DATA {
    PRESOURCE_ARBITER Arbiter;
    ULONGLONG AmountNotAllocated;
} ARBITER_ALLOCATION_ARBITER_DATA, *PARBITER_ALLOCATION_ARBITER_DATA;

/*++

Structure Description:

    This structure defines the data associated with a resource requirement
    during an arbiter allocation session.

Members:

    Requirement - Stores a pointer to the actual resource requirement.

    DeviceIndex - Stores the index into the array of context devices for the
        device that generated this requirement.

    Allocation - Stores a pointer to the arbiter allocation for the requirement.

    ArbiterIndex - Stores the index into the arbiter data array where the
        arbiter for this requirement can be found.

--*/

typedef struct _ARBITER_ALLOCATION_REQUIREMENT {
    PRESOURCE_REQUIREMENT Requirement;
    ULONG DeviceIndex;
    PARBITER_ENTRY Allocation;
    ULONG ArbiterIndex;
} ARBITER_ALLOCATION_REQUIREMENT, *PARBITER_ALLOCATION_REQUIREMENT;

/*++

Structure Description:

    This structure defines an arbiter allocation context, a scratchpad of
    state used when trying to satisfy allocations of one or more devices.

Members:

    ArbiterData - Stores an array of arbiter data structures, one for each
        arbiter involved in this allocation. This array is always deduplicated.
        Its capacity is always the resource requirement count for the worst
        case where every requirement is a different arbiter.

    ArbiterCount - Stores the number of valid elements currently in the
        arbiters array.

    Device - Stores an array of pointers to devices. These represent the
        devices involved in this set of allocations.

    CurrentDeviceConfiguration - Supplies an array of pointers to device
        configurations. For each device, this represents which of the
        possible configurations is being worked on.

    DeviceCount - Stores the number of elements in the device and current
        configuration arrays.

    Requirements - Stores an array of resource requirements and their
        associated data.

    RequirementCount - Stores the number of elements in the requirement and
        requirement device arrays.

--*/

typedef struct _ARBITER_ALLOCATION_CONTEXT {
    PARBITER_ALLOCATION_ARBITER_DATA ArbiterData;
    ULONG ArbiterCount;
    PDEVICE *Device;
    PRESOURCE_REQUIREMENT_LIST *CurrentDeviceConfiguration;
    ULONG DeviceCount;
    PARBITER_ALLOCATION_REQUIREMENT Requirements;
    ULONG RequirementCount;
} ARBITER_ALLOCATION_CONTEXT, *PARBITER_ALLOCATION_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    devres.h

Abstract:

    This header contains definitions for I/O resources.

Author:

    Evan Green 2-Dec-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define resource requirement and allocation flags.
//

//
// Set this bit if the allocation cannot be shared with any other device.
//

#define RESOURCE_FLAG_NOT_SHAREABLE 0x00000001

//
// This bit is set by the arbiter in resource allocations if the allocation was
// a boot allocation. It is ignored if passed in a requirement.
//

#define RESOURCE_FLAG_BOOT 0x00000002

//
// Define interrupt line characteristics.
//

#define INTERRUPT_LINE_EDGE_TRIGGERED 0x00000001
#define INTERRUPT_LINE_ACTIVE_LOW     0x00000002
#define INTERRUPT_LINE_ACTIVE_HIGH    0x00000004
#define INTERRUPT_LINE_WAKE           0x00000008
#define INTERRUPT_LINE_DEBOUNCE       0x00000010

//
// Define interrupt vector characteristics.
//

#define INTERRUPT_VECTOR_EDGE_TRIGGERED 0x00000001
#define INTERRUPT_VECTOR_ACTIVE_LOW     0x00000002
#define INTERRUPT_VECTOR_ACTIVE_HIGH    0x00000004

//
// Define DMA characteristics.
//

#define DMA_TYPE_ISA            0x00000001
#define DMA_TYPE_EISA_A         0x00000002
#define DMA_TYPE_EISA_B         0x00000004
#define DMA_TYPE_EISA_F         0x00000008
#define DMA_BUS_MASTER          0x00000010
#define DMA_TRANSFER_SIZE_8     0x00000020
#define DMA_TRANSFER_SIZE_16    0x00000040
#define DMA_TRANSFER_SIZE_32    0x00000080
#define DMA_TRANSFER_SIZE_64    0x00000100
#define DMA_TRANSFER_SIZE_128   0x00000200

//
// Define memory characteristics.
//

#define MEMORY_CHARACTERISTIC_PREFETCHABLE 0x00000100

//
// Define GPIO characteristics.
//

#define GPIO_CHARACTERISTIC_INTERRUPT       0x00000001
#define GPIO_CHARACTERISTIC_INPUT           0x00000002
#define GPIO_CHARACTERISTIC_OUTPUT          0x00000004
#define GPIO_CHARACTERISTIC_WAKE            0x00000008
#define GPIO_CHARACTERISTIC_ACTIVE_HIGH     0x00000010
#define GPIO_CHARACTERISTIC_ACTIVE_LOW      0x00000020
#define GPIO_CHARACTERISTIC_EDGE_TRIGGERED  0x00000040
#define GPIO_CHARACTERISTIC_PULL_UP         0x00000080
#define GPIO_CHARACTERISTIC_PULL_DOWN       0x00000100
#define GPIO_CHARACTERISTIC_PULL_NONE \
    (GPIO_CHARACTERISTIC_PULL_UP | GPIO_CHARACTERISTIC_PULL_DOWN)

#define RESOURCE_GPIO_DATA_VERSION 1
#define RESOURCE_GPIO_DEFAULT_DRIVE_STRENGTH ((ULONG)-1)
#define RESOURCE_GPIO_DEFAULT_DEBOUNCE_TIMEOUT ((ULONG)-1)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DEVICE DEVICE, *PDEVICE;

typedef enum _RESOURCE_TYPE {
    ResourceTypeInvalid,
    ResourceTypePhysicalAddressSpace,
    ResourceTypeIoPort,
    ResourceTypeInterruptLine,
    ResourceTypeInterruptVector,
    ResourceTypeBusNumber,
    ResourceTypeDmaLine,
    ResourceTypeVendorSpecific,
    ResourceTypeGpio,
    ResourceTypeCount,
} RESOURCE_TYPE, *PRESOURCE_TYPE;

/*++

Structure Description:

    This structure defines a device resource requirement.

Members:

    ListEntry - Stores pointers to the next and previous resource requirements
        in the resource requirement list.

    AlternativeListEntry - Stores pointers to the next and previous alternative
        entries that would equivalently satisfy this requirement.

    Type - Stores the type of resource being described.

    Minimum - Stores the minimum address of the range that can satisfy the
        requirement.

    Maximum - Stores the maximum address (exclusive) of the range that can
        satisfy the requirement. In other words, the first address outside the
        range.

    Alignment - Stores the byte alignment requirement of the beginning of the
        range.

    Length - Stores the minimum required length of the resource.

    Characteristics - Stores a bitfield of characteristics of the resource
        requirement. These are characteristics of the resource and must match.

    Flags - Store a bitfield of flags regarding the resource requirement. These
        bits represent properties that are not necessarily reflected in the
        final selected resource.

    OwningRequirement - Stores a pointer to an owning resource requirement
        whose allocation influences the allocation of this requirement (e.g.
        interrupt vector allocations can depend on interrupt line allocations).

    Data - Stores a pointer to the additional data for this requirement. This
        data is only required for some resource types (like GPIO).

    DataSize - Stores the size of the additional data in bytes.

    Provider - Stores an optional pointer to the device that provides the
        resource. If NULL, then the provider will be automatically determined
        by walking up the device's parents.

--*/

typedef struct _RESOURCE_REQUIREMENT RESOURCE_REQUIREMENT;
typedef struct _RESOURCE_REQUIREMENT *PRESOURCE_REQUIREMENT;
struct _RESOURCE_REQUIREMENT {
    LIST_ENTRY ListEntry;
    LIST_ENTRY AlternativeListEntry;
    RESOURCE_TYPE Type;
    ULONGLONG Minimum;
    ULONGLONG Maximum;
    ULONGLONG Alignment;
    ULONGLONG Length;
    ULONGLONG Characteristics;
    ULONGLONG Flags;
    PRESOURCE_REQUIREMENT OwningRequirement;
    PVOID Data;
    UINTN DataSize;
    PDEVICE Provider;
};

/*++

Structure Description:

    This structure defines a list of resource requirements that collectively
    represent a viable configuration for a device.

Members:

    ListEntry - Stores pointers to the next and previous resource requirement
        lists in the set.

    RequirementListHead - Stores the head of the list of resource requirements.
        The type of each entry on the list is a RESOURCE_REQUIREMENT.

--*/

typedef struct _RESOURCE_REQUIREMENT_LIST {
    LIST_ENTRY ListEntry;
    LIST_ENTRY RequirementListHead;
} RESOURCE_REQUIREMENT_LIST, *PRESOURCE_REQUIREMENT_LIST;

/*++

Structure Description:

    This structure defines a list of resource requirements that collectively
    represent a viable configuration for a device.

Members:

    RequirementListListHead - Stores the list head of possible resource
        configurations, ordered by preference (ListHead.Next representing the
        most desirable configuration).

--*/

typedef struct _RESOURCE_CONFIGURATION_LIST {
    LIST_ENTRY RequirementListListHead;
} RESOURCE_CONFIGURATION_LIST, *PRESOURCE_CONFIGURATION_LIST;

/*++

Structure Description:

    This structure defines a resource allocation.

Members:

    ListEntry - Stores pointers to the next and previous resource allocations
        in the resource allocation list.

    Type - Stores the type of resource being described.

    Allocation - Stores the base value of the allocation, which can be a
        base physical address, I/O port, interrupt pin, etc.

    Length - Stores the length of the resource allocation.

    Characteristics - Stores a bitfield of characteristics of the resource.

    Flags - Store a bitfield of flags regarding the resource.

    OwningAllocation - Stores a pointer to an owning resource allocation whose
        allocation dictates the allocation of this resource (e.g. interrupt
        vector allocations can depend on interrupt line allocations).

    Data - Stores a pointer to the additional data for this requirement. This
        data is only required for some resource types (like GPIO).

    DataSize - Stores the size of the additional data in bytes.

    Provider - Stores an optional pointer to the device providing the resource.
        If NULL, then the provider will be automatically determined by walking
        up the device's parents.

--*/

typedef struct _RESOURCE_ALLOCATION RESOURCE_ALLOCATION, *PRESOURCE_ALLOCATION;
struct _RESOURCE_ALLOCATION {
    LIST_ENTRY ListEntry;
    RESOURCE_TYPE Type;
    ULONGLONG Allocation;
    ULONGLONG Length;
    ULONGLONG Characteristics;
    ULONGLONG Flags;
    PRESOURCE_ALLOCATION OwningAllocation;
    PVOID Data;
    UINTN DataSize;
    PDEVICE Provider;
};

/*++

Structure Description:

    This structure defines a list of resources allocated to a particular device.
    The order of resources in this list will match the order of resource
    requirements in the resource requirements list.

Members:

    AllocationListHead - Stores the head of the list of allocated resources for
        the device. The type of entries on this list is RESOURCE_ALLOCATION.

--*/

typedef struct _RESOURCE_ALLOCATION_LIST {
    LIST_ENTRY AllocationListHead;
} RESOURCE_ALLOCATION_LIST, *PRESOURCE_ALLOCATION_LIST;

/*++

Structure Description:

    This structure defines the contents of the additional data stored along
    with a GPIO resource.

Members:

    Version - Stores the constant RESOURCE_GPIO_DATA_VERSION.

    OutputDriveStrength - Stores the output drive strength for the GPIO
        resource in units of microamperes.

    DebounceTimeout - Stores the debounce timeout value for the GPIO resource
        in units of microseconds.

    VendorData - Stores a pointer to the vendor data for this resource. This
        data is probably immediately after the structure itself.

    VendorDataSize - Stores the size of the vendor data for this resource.

--*/

typedef struct _RESOURCE_GPIO_DATA {
    ULONG Version;
    ULONG OutputDriveStrength;
    ULONG DebounceTimeout;
    PVOID VendorData;
    UINTN VendorDataSize;
} RESOURCE_GPIO_DATA, *PRESOURCE_GPIO_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Resource requirement list support routines.
//

KERNEL_API
PRESOURCE_REQUIREMENT_LIST
IoCreateResourceRequirementList (
    VOID
    );

/*++

Routine Description:

    This routine creates a new empty resource requirement list.

Arguments:

    None.

Return Value:

    Returns a pointer to the new resource requirement list on success.

    NULL on allocation failure.

--*/

KERNEL_API
VOID
IoDestroyResourceRequirementList (
    PRESOURCE_REQUIREMENT_LIST ResourceRequirementList
    );

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

KERNEL_API
KSTATUS
IoCreateAndAddResourceRequirement (
    PRESOURCE_REQUIREMENT Requirement,
    PRESOURCE_REQUIREMENT_LIST ResourceRequirementList,
    PRESOURCE_REQUIREMENT *NewRequirement
    );

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

KERNEL_API
VOID
IoRemoveResourceRequirement (
    PRESOURCE_REQUIREMENT Requirement
    );

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

KERNEL_API
KSTATUS
IoCreateAndAddResourceRequirementAlternative (
    PRESOURCE_REQUIREMENT Alternative,
    PRESOURCE_REQUIREMENT Requirement
    );

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

KERNEL_API
VOID
IoRemoveResourceRequirementAlternative (
    PRESOURCE_REQUIREMENT Alternative
    );

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

KERNEL_API
KSTATUS
IoCreateAndAddInterruptVectorsForLines (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList,
    PRESOURCE_REQUIREMENT VectorTemplate
    );

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

KERNEL_API
PRESOURCE_REQUIREMENT
IoGetNextResourceRequirement (
    PRESOURCE_REQUIREMENT_LIST ResourceRequirementList,
    PRESOURCE_REQUIREMENT CurrentEntry
    );

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

KERNEL_API
PRESOURCE_REQUIREMENT
IoGetNextResourceRequirementAlternative (
    PRESOURCE_REQUIREMENT ResourceRequirement,
    PRESOURCE_REQUIREMENT CurrentEntry
    );

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

//
// Resource configuration list routines.
//

KERNEL_API
PRESOURCE_CONFIGURATION_LIST
IoCreateResourceConfigurationList (
    PRESOURCE_REQUIREMENT_LIST FirstConfiguration
    );

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

KERNEL_API
VOID
IoDestroyResourceConfigurationList (
    PRESOURCE_CONFIGURATION_LIST ResourceConfigurationList
    );

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

KERNEL_API
KSTATUS
IoAddResourceConfiguration (
    PRESOURCE_REQUIREMENT_LIST Configuration,
    PRESOURCE_REQUIREMENT_LIST ConfigurationToInsertAfter,
    PRESOURCE_CONFIGURATION_LIST ConfigurationList
    );

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

KERNEL_API
VOID
IoRemoveResourceConfiguration (
    PRESOURCE_REQUIREMENT_LIST Configuration,
    PRESOURCE_CONFIGURATION_LIST ConfigurationList
    );

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

KERNEL_API
PRESOURCE_REQUIREMENT_LIST
IoGetNextResourceConfiguration (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList,
    PRESOURCE_REQUIREMENT_LIST CurrentEntry
    );

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

//
// Resource allocation list support routines.
//

KERNEL_API
PRESOURCE_ALLOCATION_LIST
IoCreateResourceAllocationList (
    VOID
    );

/*++

Routine Description:

    This routine creates a new empty resource allocation list.

Arguments:

    None.

Return Value:

    Returns a pointer to the new resource allocation list on success.

    NULL on allocation failure.

--*/

KERNEL_API
VOID
IoDestroyResourceAllocationList (
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList
    );

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

KERNEL_API
KSTATUS
IoCreateAndAddResourceAllocation (
    PRESOURCE_ALLOCATION Allocation,
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList
    );

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

KERNEL_API
VOID
IoRemoveResourceAllocation (
    PRESOURCE_ALLOCATION Allocation,
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList
    );

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

KERNEL_API
PRESOURCE_ALLOCATION
IoGetNextResourceAllocation (
    PRESOURCE_ALLOCATION_LIST ResourceAllocationList,
    PRESOURCE_ALLOCATION CurrentEntry
    );

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

KERNEL_API
VOID
IoDebugPrintResourceConfigurationList (
    PRESOURCE_CONFIGURATION_LIST ConfigurationList
    );

/*++

Routine Description:

    This routine prints a resource configuration list out to the debugger.

Arguments:

    ConfigurationList - Supplies a pointer to the resource configuration list to
        print.

Return Value:

    None.

--*/

KERNEL_API
VOID
IoDebugPrintResourceRequirementList (
    ULONG IndentationLevel,
    PRESOURCE_REQUIREMENT_LIST RequirementList
    );

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

KERNEL_API
VOID
IoDebugPrintResourceRequirement (
    ULONG IndentationLevel,
    PRESOURCE_REQUIREMENT Requirement
    );

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

KERNEL_API
VOID
IoDebugPrintResourceAllocationList (
    ULONG IndentationLevel,
    PRESOURCE_ALLOCATION_LIST AllocationList
    );

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

KERNEL_API
VOID
IoDebugPrintResourceAllocation (
    ULONG IndentationLevel,
    PRESOURCE_ALLOCATION Allocation
    );

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


/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hmodapi.c

Abstract:

    This module implements the system services used by hardware modules.

Author:

    Evan Green 7-Aug-2013

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "../dbgdev.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the initial memory pool, which needs to get the hardware
// library by until phase 0 initialization, when the memory manager is online.
//

#define BOOT_HL_INITIAL_POOL_SIZE 256

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
BoAllocateMemory (
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the kernel initialization block. This pointer can only
// be touched during early boot, as the parameter block disappears at some
// point.
//

PKERNEL_INITIALIZATION_BLOCK BoHlKernelParameters = NULL;

//
// Define the initial memory pool, which satisfies requests until it is
// exhausted, at which point non-paged pool is used.
//

UCHAR BoHlInitialPool[BOOT_HL_INITIAL_POOL_SIZE];

//
// Define the pointer to the next pool allocation.
//

PUCHAR BoHlInitialPoolNextAllocation = BoHlInitialPool;

//
// Define the remaining size of the initial pool.
//

ULONG BoHlInitialPoolRemainingSize = BOOT_HL_INITIAL_POOL_SIZE;

//
// Store the list head for the physical address usage registered by the
// hardware modules. This will be a list of HL_PHYSICAL_ADDRESS_USAGE
// structures.
//

LIST_ENTRY BoHlPhysicalMemoryUsageListHead;

//
// Store whether or not the USB host controllers have been enumerated.
//

extern BOOL HlUsbHostsEnumerated;

//
// Store a pointer to the optional get ACPI table override routine.
//

PHARDWARE_MODULE_GET_ACPI_TABLE BoHlGetAcpiTableFunction;

//
// ------------------------------------------------------------------ Functions
//

PLIST_ENTRY
BoHlGetPhysicalMemoryUsageListHead (
    VOID
    )

/*++

Routine Description:

    This routine returns the head of the list of regions of physical address
    space in use by the hardware layer.

Arguments:

    None.

Return Value:

    Returns a pointer to a list head pointing to a list of
    HL_PHYSICAL_ADDRESS_USAGE structures. Note that the first entry (the value
    returned) is not an entry itself but just the list head. The first valid
    entry comes from ReturnValue->Next.

--*/

{

    return &BoHlPhysicalMemoryUsageListHead;
}

KSTATUS
BoHlBootInitialize (
    PDEBUG_DEVICE_DESCRIPTION *DebugDevice,
    PHARDWARE_MODULE_GET_ACPI_TABLE GetAcpiTableFunction
    )

/*++

Routine Description:

    This routine initializes the boot hardware library.

Arguments:

    DebugDevice - Supplies a pointer where a pointer to the debug device
        description will be returned on success.

    GetAcpiTableFunction - Supplies an optional pointer to a function used to
        get ACPI tables. If not supplied a default hardware module service
        will be used that always returns NULL.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    INITIALIZE_LIST_HEAD(&BoHlPhysicalMemoryUsageListHead);
    BoHlGetAcpiTableFunction = GetAcpiTableFunction;
    Status = HlpInitializeDebugDevices(0, DebugDevice);
    return Status;
}

VOID
BoHlTestUsbDebugInterface (
    VOID
    )

/*++

Routine Description:

    This routine runs the interface test on a USB debug interface if debugging
    the USB transport itself.

Arguments:

    None.

Return Value:

    None.

--*/

{

    HlUsbHostsEnumerated = FALSE;
    HlpTestUsbDebugInterface();
    return;
}

KERNEL_API
KSTATUS
HlRegisterHardware (
    HARDWARE_MODULE_TYPE Type,
    PVOID Description
    )

/*++

Routine Description:

    This routine registers a hardware module with the system.

Arguments:

    Type - Supplies the type of resource being registered.

    Description - Supplies a description of the resource being registered.

Return Value:

    Returns a pointer to the allocation of the requested size on success.

    NULL on failure.

--*/

{

    KSTATUS Status;

    switch (Type) {
    case HardwareModuleDebugDevice:
        Status = HlpDebugDeviceRegisterHardware(Description);
        break;

    case HardwareModuleDebugUsbHostController:
        Status = HlpDebugUsbHostRegisterHardware(Description);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto RegisterHardwareEnd;
    }

RegisterHardwareEnd:
    return Status;
}

KERNEL_API
PVOID
HlGetAcpiTable (
    ULONG Signature,
    PVOID PreviousTable
    )

/*++

Routine Description:

    This routine attempts to find an ACPI description table with the given
    signature.

Arguments:

    Signature - Supplies the signature of the desired table.

    PreviousTable - Supplies a pointer to the table to start the search from.

Return Value:

    Returns a pointer to the beginning of the header to the table if the table
    was found, or NULL if the table could not be located.

--*/

{

    PHARDWARE_MODULE_GET_ACPI_TABLE GetAcpiTable;

    GetAcpiTable = BoHlGetAcpiTableFunction;
    if (GetAcpiTable != NULL) {
        return GetAcpiTable(Signature, PreviousTable);
    }

    return NULL;
}

KERNEL_API
PVOID
HlAllocateMemory (
    UINTN Size,
    ULONG Tag,
    BOOL Device,
    PPHYSICAL_ADDRESS PhysicalAddress
    )

/*++

Routine Description:

    This routine allocates memory from the non-paged pool. This memory will
    never be paged out and can be accessed at any level.

Arguments:

    Size - Supplies the size of the allocation, in bytes.

    Tag - Supplies an identifier to associate with the allocation, useful for
        debugging and leak detection.

    Device - Supplies a boolean indicating if this memory will be accessed by
        a device directly. If TRUE, the memory will be mapped uncached.

    PhysicalAddress - Supplies an optional pointer where the physical address
        of the allocation is returned.

Return Value:

    Returns the allocated memory if successful, or NULL on failure.

--*/

{

    PVOID Allocation;

    if (Size <= BoHlInitialPoolRemainingSize) {
        Allocation = BoHlInitialPoolNextAllocation;
        BoHlInitialPoolNextAllocation += Size;
        BoHlInitialPoolRemainingSize -= Size;

    } else {
        Allocation = BoAllocateMemory(Size);
    }

    if (PhysicalAddress != NULL) {
        *PhysicalAddress = (UINTN)Allocation;
    }

    return Allocation;
}

KERNEL_API
PVOID
HlMapPhysicalAddress (
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONG SizeInBytes,
    BOOL CacheDisabled
    )

/*++

Routine Description:

    This routine maps a physical address into kernel VA space. It is meant so
    that system components can access memory mapped hardware.

Arguments:

    PhysicalAddress - Supplies a pointer to the physical address. This address
        must be page aligned.

    SizeInBytes - Supplies the size in bytes of the mapping. This will be
        rounded up to the nearest page size.

    CacheDisabled - Supplies a boolean indicating if the memory is to be mapped
        uncached.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

{

    return (PVOID)(UINTN)PhysicalAddress;
}

KERNEL_API
VOID
HlUnmapAddress (
    PVOID VirtualAddress,
    ULONG SizeInBytes
    )

/*++

Routine Description:

    This routine unmaps memory mapped with MmMapPhysicalMemory.

Arguments:

    VirtualAddress - Supplies the virtual address to unmap.

    SizeInBytes - Supplies the number of bytes to unmap.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
VOID
HlReportPhysicalAddressUsage (
    PHYSICAL_ADDRESS PhysicalAddress,
    ULONGLONG Size
    )

/*++

Routine Description:

    This routine is called by a hardware module plugin to notify the system
    about a range of physical address space that is in use by that hardware
    plugin. This helps notify the system to avoid using this address space
    when configuring devices that can remap their memory windows. This function
    should be called during the discovery portion, as it is relevant to the
    system regardless of whether that hardware module is actually initialized
    and used.

Arguments:

    PhysicalAddress - Supplies the first physical address in use by the hardware
        module.

    Size - Supplies the size of the memory segment, in bytes.

Return Value:

    None.

--*/

{

    PHL_PHYSICAL_ADDRESS_USAGE Usage;

    Usage = HlAllocateMemory(sizeof(HL_PHYSICAL_ADDRESS_USAGE),
                             HL_POOL_TAG,
                             FALSE,
                             NULL);

    if (Usage == NULL) {
        return;
    }

    RtlZeroMemory(Usage, sizeof(HL_PHYSICAL_ADDRESS_USAGE));
    Usage->PhysicalAddress = PhysicalAddress;
    Usage->Size = Size;
    INSERT_BEFORE(&(Usage->ListEntry), &BoHlPhysicalMemoryUsageListHead);
    return;
}

KERNEL_API
VOID
HlInitializeLock (
    PHARDWARE_MODULE_LOCK Lock
    )

/*++

Routine Description:

    This routine initializes a hardware module lock structure. This must be
    called before the lock can be acquired or released.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

Return Value:

    None.

--*/

{

    RtlZeroMemory(Lock, sizeof(HARDWARE_MODULE_LOCK));
    return;
}

KERNEL_API
VOID
HlAcquireLock (
    PHARDWARE_MODULE_LOCK Lock
    )

/*++

Routine Description:

    This routine disables interrupts and acquires a high level spin lock.
    Callers should be very careful to avoid doing this in hot paths or for
    very long. This lock is not reentrant.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

{

    return;
}

KERNEL_API
VOID
HlReleaseLock (
    PHARDWARE_MODULE_LOCK Lock
    )

/*++

Routine Description:

    This routine releases a previously acquired high level lock and restores
    interrupts to their previous state.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


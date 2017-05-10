/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hmodapi.c

Abstract:

    This module implements the kernel services used by system hardware modules.

Author:

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "hlp.h"
#include "cache.h"
#include "calendar.h"
#include "intrupt.h"
#include "dbgdev.h"
#include "timer.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the kernel initialization block. This pointer can only
// be touched during early boot, as the parameter block disappears at some
// point.
//

PKERNEL_INITIALIZATION_BLOCK HlModKernelParameters = NULL;

//
// Define the memory region to dole out for hardware module allocation
// requests.
//

PVOID HlModPool = NULL;
PHYSICAL_ADDRESS HlModPoolPhysical;
UINTN HlModPoolSize = 0;

PVOID HlModPoolDevice = NULL;
PHYSICAL_ADDRESS HlModPoolDevicePhysical;
UINTN HlModPoolDeviceSize = 0;

//
// Store the list head for the physical address usage registered by the
// hardware modules. This will be a list of HL_PHYSICAL_ADDRESS_USAGE
// structures.
//

LIST_ENTRY HlModPhysicalMemoryUsageListHead;

//
// ------------------------------------------------------------------ Functions
//

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
    case HardwareModuleInterruptController:
        Status = HlpInterruptRegisterHardware(Description, RunLevelCount, NULL);
        break;

    case HardwareModuleInterruptLines:
        Status = HlpInterruptRegisterLines(Description);
        break;

    case HardwareModuleTimer:
        Status = HlpTimerRegisterHardware(Description);
        break;

    case HardwareModuleDebugDevice:
        Status = HlpDebugDeviceRegisterHardware(Description);
        break;

    case HardwareModuleCalendarTimer:
        Status = HlpCalendarTimerRegisterHardware(Description);
        break;

    case HardwareModuleCacheController:
        Status = HlpCacheControllerRegisterHardware(Description);
        break;

    case HardwareModuleDebugUsbHostController:
        Status = HlpDebugUsbHostRegisterHardware(Description);
        break;

    case HardwareModuleReboot:
        Status = HlpRebootModuleRegisterHardware(Description);
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

    return AcpiFindTable(Signature, PreviousTable);
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
    UINTN AllocationSize;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;

    Allocation = NULL;
    Size = ALIGN_RANGE_UP(Size, 8);
    if (Device != FALSE) {

        //
        // Allocate more uncached space if needed.
        //

        if (Size > HlModPoolDeviceSize) {
            AllocationSize = ALIGN_RANGE_UP(Size, MmPageSize());
            IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
                            IO_BUFFER_FLAG_MAP_NON_CACHED;

            IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                  MAX_ULONGLONG,
                                                  0,
                                                  AllocationSize,
                                                  IoBufferFlags);

            if (IoBuffer != NULL) {
                HlModPoolDevice = IoBuffer->Fragment[0].VirtualAddress;
                HlModPoolDevicePhysical = IoBuffer->Fragment[0].PhysicalAddress;
                HlModPoolDeviceSize = IoBuffer->Fragment[0].Size;
            }
        }

        if (HlModPoolDeviceSize >= Size) {
            Allocation = HlModPoolDevice;
            if (PhysicalAddress != NULL) {
                *PhysicalAddress = HlModPoolDevicePhysical;
            }

            HlModPoolDevice += Size;
            HlModPoolDevicePhysical += Size;
            HlModPoolDeviceSize -= Size;
        }

    //
    // Allocate from non-device memory.
    //

    } else {

        //
        // Allocate more cached space if needed.
        //

        if (Size > HlModPoolSize) {
            AllocationSize = ALIGN_RANGE_UP(Size, MmPageSize());
            IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
            IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                  MAX_ULONGLONG,
                                                  0,
                                                  AllocationSize,
                                                  IoBufferFlags);

            if (IoBuffer != NULL) {
                HlModPool = IoBuffer->Fragment[0].VirtualAddress;
                HlModPoolPhysical = IoBuffer->Fragment[0].PhysicalAddress;
                HlModPoolSize = IoBuffer->Fragment[0].Size;
            }
        }

        if (HlModPoolSize >= Size) {
            Allocation = HlModPool;
            if (PhysicalAddress != NULL) {
                *PhysicalAddress = HlModPoolPhysical;
            }

            HlModPool += Size;
            HlModPoolPhysical += Size;
            HlModPoolSize -= Size;
        }
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

    PHYSICAL_ADDRESS AlignedAddress;
    UINTN AlignedSize;
    PLIST_ENTRY CurrentEntry;
    PSYSTEM_RESOURCE_HEADER GenericEntry;
    ULONG Offset;
    PVOID VirtualAddress;

    AlignedAddress = ALIGN_RANGE_DOWN(PhysicalAddress, MmPageSize());
    Offset = PhysicalAddress - AlignedAddress;
    AlignedSize = SizeInBytes + Offset;

    //
    // If translation is not even enabled, just return the physical address.
    //

    if (ArIsTranslationEnabled() == FALSE) {
        return (PVOID)(UINTN)PhysicalAddress;
    }

    //
    // Attempt to find the resource in the list of system resources.
    //

    if (HlModKernelParameters != NULL) {
        CurrentEntry = HlModKernelParameters->SystemResourceListHead.Next;
        while (CurrentEntry !=
               &(HlModKernelParameters->SystemResourceListHead)) {

            GenericEntry =
                   LIST_VALUE(CurrentEntry, SYSTEM_RESOURCE_HEADER, ListEntry);

            CurrentEntry = CurrentEntry->Next;
            if ((GenericEntry->PhysicalAddress == PhysicalAddress) &&
                (GenericEntry->Size >= SizeInBytes) &&
                (GenericEntry->VirtualAddress != NULL)) {

                return GenericEntry->VirtualAddress;
            }

            if ((GenericEntry->PhysicalAddress == AlignedAddress) &&
                (GenericEntry->Size >= AlignedSize) &&
                (GenericEntry->VirtualAddress != NULL)) {

                return GenericEntry->VirtualAddress + Offset;
            }
        }
    }

    //
    // This area of memory has not yet been mapped, so call MM to map it.
    //

    VirtualAddress = MmMapPhysicalAddress(AlignedAddress,
                                          AlignedSize,
                                          TRUE,
                                          FALSE,
                                          CacheDisabled);

    if (VirtualAddress == NULL) {
        return NULL;
    }

    return VirtualAddress + Offset;
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

    return MmUnmapAddress(VirtualAddress, SizeInBytes);
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
    INSERT_BEFORE(&(Usage->ListEntry), &HlModPhysicalMemoryUsageListHead);
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

    Lock->WasEnabled = ArDisableInterrupts();
    while (RtlAtomicCompareExchange32(&(Lock->Value), 1, 0) == 1) {
        ArProcessorYield();
    }

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

    ULONG OldValue;

    OldValue = RtlAtomicExchange32(&(Lock->Value), 0);

    ASSERT(OldValue == 1);

    if (Lock->WasEnabled != FALSE) {
        ArEnableInterrupts();
    }

    return;
}

VOID
HlpModInitializePreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine implements early initialization for the hardware module API
    layer. This routine is *undebuggable*, as it is called before the debugger
    is brought online.

Arguments:

    Parameters - Supplies an optional pointer to the kernel initialization
        parameters. This parameter may be NULL.

    ProcessorNumber - Supplies the processor index of this processor.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSYSTEM_RESOURCE_MEMORY Memory;
    PSYSTEM_RESOURCE_MEMORY Pool;
    PSYSTEM_RESOURCE_MEMORY PoolDevice;
    PSYSTEM_RESOURCE_HEADER Resource;

    if (ProcessorNumber == 0) {
        INITIALIZE_LIST_HEAD(&HlModPhysicalMemoryUsageListHead);

        //
        // Go find the resource created by the loader for satisfying
        // allocations initially.
        //

        Pool = NULL;
        PoolDevice = NULL;
        CurrentEntry = Parameters->SystemResourceListHead.Next;
        while (CurrentEntry != &(Parameters->SystemResourceListHead)) {
            Resource = LIST_VALUE(CurrentEntry,
                                  SYSTEM_RESOURCE_HEADER,
                                  ListEntry);

            if ((Resource->Type == SystemResourceMemory) &&
                (Resource->Acquired == FALSE)) {

                Memory = PARENT_STRUCTURE(Resource,
                                          SYSTEM_RESOURCE_MEMORY,
                                          Header);

                if (Memory->MemoryType == SystemMemoryResourceHardwareModule) {
                    Pool = Memory;
                    Pool->Header.Acquired = TRUE;
                    LIST_REMOVE(&(Pool->Header.ListEntry));

                } else if (Memory->MemoryType ==
                           SystemMemoryResourceHardwareModuleDevice) {

                    PoolDevice = Memory;
                    PoolDevice->Header.Acquired = TRUE;
                    LIST_REMOVE(&(PoolDevice->Header.ListEntry));
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (Pool != NULL) {
            HlModPool = Pool->Header.VirtualAddress;
            HlModPoolPhysical = Pool->Header.PhysicalAddress;
            HlModPoolSize = Pool->Header.Size;
        }

        if (PoolDevice != NULL) {
            HlModPoolDevice = PoolDevice->Header.VirtualAddress;
            HlModPoolDevicePhysical = PoolDevice->Header.PhysicalAddress;
            HlModPoolDeviceSize = PoolDevice->Header.Size;
        }
    }

    if (Parameters != NULL) {
        HlModKernelParameters = Parameters;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


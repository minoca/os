/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memory.c

Abstract:

    This module implements UEFI-specific memory management support.

Author:

    Evan Green 11-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/uefi/uefi.h>
#include "firmware.h"
#include <minoca/lib/basevid.h>
#include "bootlib.h"
#include "efisup.h"
#include "paging.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_EXIT_BOOT_SERVICES_TRY_COUNT 4

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BopEfiMapInitialDescriptorAllocation (
    VOID
    );

KSTATUS
BopEfiMapRuntimeServices (
    UINTN *MemoryMapSize,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR **VirtualMap
    );

KSTATUS
BopMapRamDisk (
    PHYSICAL_ADDRESS Base,
    ULONGLONG Size,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BoFwMapKnownRegions (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine maps known ARM regions of memory.

Arguments:

    Phase - Supplies the phase number, as this routine is called twice: once
        before any other mappings have been established (0), and once near the
        end of the loader (1).

    Parameters - Supplies a pointer to the kernel's initialization parameters.

Return Value:

    Status code.

--*/

{

    PSYSTEM_RESOURCE_FRAME_BUFFER FrameBufferResource;
    ULONG RamDiskCount;
    ULONG RamDiskIndex;
    PBOOT_RAM_DISK RamDisks;
    KSTATUS Status;

    RamDisks = NULL;
    FrameBufferResource = NULL;
    if (Phase == 0) {
        Status = BopEfiMapInitialDescriptorAllocation();
        if (!KSUCCESS(Status)) {
            goto MapKnownRegionsEnd;
        }

    } else {

        ASSERT(Phase == 1);

        //
        // Map the frame buffer. It's okay if the frame buffer doesn't exist.
        //

        FrameBufferResource =
                        BoAllocateMemory(sizeof(SYSTEM_RESOURCE_FRAME_BUFFER));

        if (FrameBufferResource == NULL) {
            Status = STATUS_NO_MEMORY;
            goto MapKnownRegionsEnd;
        }

        RtlZeroMemory(FrameBufferResource,
                      sizeof(SYSTEM_RESOURCE_FRAME_BUFFER));

        FrameBufferResource->Header.Type = SystemResourceFrameBuffer;
        Status = BopEfiGetVideoInformation(
                                &(FrameBufferResource->Width),
                                &(FrameBufferResource->Height),
                                &(FrameBufferResource->PixelsPerScanLine),
                                &(FrameBufferResource->BitsPerPixel),
                                &(FrameBufferResource->RedMask),
                                &(FrameBufferResource->GreenMask),
                                &(FrameBufferResource->BlueMask),
                                &(FrameBufferResource->Header.PhysicalAddress),
                                &(FrameBufferResource->Header.Size));

        if (KSUCCESS(Status)) {
            FrameBufferResource->Mode = BaseVideoModeFrameBuffer;
            FrameBufferResource->Header.VirtualAddress = (PVOID)-1;
            Status = BoMapPhysicalAddress(
                                 &(FrameBufferResource->Header.VirtualAddress),
                                 FrameBufferResource->Header.PhysicalAddress,
                                 FrameBufferResource->Header.Size,
                                 MAP_FLAG_WRITE_THROUGH | MAP_FLAG_GLOBAL,
                                 MemoryTypeLoaderPermanent);

            if (!KSUCCESS(Status)) {
                goto MapKnownRegionsEnd;
            }

            INSERT_BEFORE(&(FrameBufferResource->Header.ListEntry),
                          &(Parameters->SystemResourceListHead));

        } else {
            BoFreeMemory(FrameBufferResource);
        }

        FrameBufferResource = NULL;

        //
        // Get a list of all the RAM disks, and map them.
        //

        Status = FwGetRamDisks(&RamDisks, &RamDiskCount);
        if (KSUCCESS(Status)) {
            for (RamDiskIndex = 0;
                 RamDiskIndex < RamDiskCount;
                 RamDiskIndex += 1) {

                BopMapRamDisk(RamDisks[RamDiskIndex].Base,
                              RamDisks[RamDiskIndex].Size,
                              Parameters);
            }
        }
    }

    Status = STATUS_SUCCESS;

MapKnownRegionsEnd:
    if (RamDisks != NULL) {
        BoFreeMemory(RamDisks);
    }

    if (FrameBufferResource != NULL) {
        BoFreeMemory(FrameBufferResource);
    }

    return Status;
}

KSTATUS
BoFwPrepareForKernelLaunch (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine coordinates with the firmware to end boot services and
    prepare for the operating system to take over. Translation is still
    disabled (or identity mapped) at this point.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

{

    UINTN EfiDescriptorSize;
    UINT32 EfiDescriptorVersion;
    EFI_MEMORY_DESCRIPTOR *EfiMap;
    UINTN EfiMapKey;
    UINTN EfiMapSize;
    EFI_STATUS EfiStatus;
    KSTATUS Status;
    ULONG Try;

    Parameters->FirmwareType = SystemFirmwareEfi;

    //
    // Stop the debugger from using stall.
    //

    KdSetConnectionTimeout(MAX_ULONG);

    //
    // Create mappings for all runtime services.
    //

    Status = BopEfiMapRuntimeServices(&EfiMapSize,
                                      &EfiDescriptorSize,
                                      &EfiDescriptorVersion,
                                      &EfiMap);

    if (!KSUCCESS(Status)) {
        goto PrepareForKernelLaunchEnd;
    }

    //
    // Loop attempting to synchronize the memory map, and exit boot services.
    // This can fail if the EFI memory map changes in between getting it and
    // exiting, though that should be rare.
    //

    for (Try = 0; Try < EFI_EXIT_BOOT_SERVICES_TRY_COUNT; Try += 1) {
        Status = BopEfiSynchronizeMemoryMap(&EfiMapKey);
        if (!KSUCCESS(Status)) {
            goto PrepareForKernelLaunchEnd;
        }

        EfiStatus = BopEfiExitBootServices(BoEfiImageHandle, EfiMapKey);
        Status = BopEfiStatusToKStatus(EfiStatus);
        if (!EFI_ERROR(EfiStatus)) {
            break;
        }
    }

    if (!KSUCCESS(Status)) {
        goto PrepareForKernelLaunchEnd;
    }

    //
    // Boot services are no longer available.
    //

    BoEfiBootServices = NULL;

    //
    // Virtualize the runtime services.
    //

    Status = BopEfiVirtualizeFirmwareServices(EfiMapSize,
                                              EfiDescriptorSize,
                                              EfiDescriptorVersion,
                                              EfiMap);

    if (!KSUCCESS(Status)) {
        goto PrepareForKernelLaunchEnd;
    }

    //
    // Save the system table for the kernel.
    //

    Parameters->EfiRuntimeServices = BoEfiSystemTable->RuntimeServices;

PrepareForKernelLaunchEnd:
    if (EfiMap != NULL) {
        BoFreeMemory(EfiMap);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BopEfiMapInitialDescriptorAllocation (
    VOID
    )

/*++

Routine Description:

    This routine maps the initial memory descriptor allocation.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    PVOID VirtualAddress;

    ASSERT((UINTN)BoEfiDescriptorAllocation == BoEfiDescriptorAllocation);

    VirtualAddress = (PVOID)(UINTN)BoEfiDescriptorAllocation;
    Status = BoMapPhysicalAddress(
                          &VirtualAddress,
                          BoEfiDescriptorAllocation,
                          BoEfiDescriptorAllocationPageCount << EFI_PAGE_SHIFT,
                          0,
                          MemoryTypeLoaderTemporary);

    if (!KSUCCESS(Status)) {
        goto EfiMapInitialDescriptorAllocationEnd;
    }

    Status = STATUS_SUCCESS;

EfiMapInitialDescriptorAllocationEnd:
    return Status;
}

KSTATUS
BopEfiMapRuntimeServices (
    UINTN *MemoryMapSize,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR **VirtualMap
    )

/*++

Routine Description:

    This routine maps any runtime services code or data into the kernel VA
    space.

Arguments:

    MemoryMapSize - Supplies a pointer where the memory map size will be
        returned on success.

    DescriptorSize - Supplies a pointer where the size of an
        EFI_MEMORY_DESCRIPTOR will be returned on success.

    DescriptorVersion - Supplies a pointer where the descriptor version will
        be returned on success.

    VirtualMap - Supplies a pointer where a pointer to the the virtual map will
        be returned on success.

Return Value:

    Status code.

--*/

{

    ULONG Attributes;
    UINTN DescriptorIndex;
    ULONGLONG EfiAttributes;
    EFI_MEMORY_DESCRIPTOR *EfiDescriptor;
    EFI_MEMORY_DESCRIPTOR *EfiMap;
    UINTN EfiMapKey;
    ULONGLONG Size;
    KSTATUS Status;
    PVOID VirtualAddress;

    EfiMap = NULL;
    Status = BopEfiGetAllocatedMemoryMap(MemoryMapSize,
                                         &EfiMap,
                                         &EfiMapKey,
                                         DescriptorSize,
                                         DescriptorVersion);

    if (!KSUCCESS(Status)) {
        goto EfiMapRuntimeServicesEnd;
    }

    //
    // Loop through each descriptor in the memory map.
    //

    DescriptorIndex = 0;
    while ((DescriptorIndex + 1) * *DescriptorSize <= *MemoryMapSize) {
        EfiDescriptor = (EFI_MEMORY_DESCRIPTOR *)((PVOID)EfiMap +
                                        (DescriptorIndex * *DescriptorSize));

        DescriptorIndex += 1;
        EfiAttributes = EfiDescriptor->Attribute;

        //
        // Skip it if it's not a runtime descriptor.
        //

        if ((EfiAttributes & EFI_MEMORY_RUNTIME) == 0) {
            continue;
        }

        Attributes = MAP_FLAG_GLOBAL | MAP_FLAG_EXECUTE;
        if ((EfiAttributes &
             (EFI_MEMORY_UC | EFI_MEMORY_UCE | EFI_MEMORY_WC)) != 0) {

            Attributes |= MAP_FLAG_CACHE_DISABLE;
        }

        if ((EfiAttributes & EFI_MEMORY_WT) != 0) {
            Attributes |= MAP_FLAG_WRITE_THROUGH;
        }

        if ((EfiAttributes & EFI_MEMORY_WP) != 0) {
            Attributes |= MAP_FLAG_READ_ONLY;
        }

        Size = EfiDescriptor->NumberOfPages << EFI_PAGE_SHIFT;

        ASSERT((UINTN)Size == Size);

        VirtualAddress = (PVOID)-1;
        Status = BoMapPhysicalAddress(&VirtualAddress,
                                      EfiDescriptor->PhysicalStart,
                                      Size,
                                      Attributes,
                                      MemoryTypeFirmwarePermanent);

        if (!KSUCCESS(Status)) {
            goto EfiMapRuntimeServicesEnd;
        }

        EfiDescriptor->VirtualStart = (UINTN)VirtualAddress;
    }

    Status = STATUS_SUCCESS;

EfiMapRuntimeServicesEnd:
    if (!KSUCCESS(Status)) {
        if (EfiMap != NULL) {
            BoFreeMemory(EfiMap);
            EfiMap = NULL;
        }
    }

    *VirtualMap = EfiMap;
    return Status;
}

KSTATUS
BopMapRamDisk (
    PHYSICAL_ADDRESS Base,
    ULONGLONG Size,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine maps a RAM disk at the given address.

Arguments:

    Base - Supplies the base physical address of the RAM disk.

    Size - Supplies the size of the RAM disk in bytes.

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

{

    PHYSICAL_ADDRESS AlignedBase;
    ULONGLONG AlignedSize;
    MEMORY_DESCRIPTOR Descriptor;
    ULONG PageOffset;
    ULONG PageSize;
    PSYSTEM_RESOURCE_RAM_DISK RamDisk;
    KSTATUS Status;

    PageSize = MmPageSize();
    RamDisk = BoAllocateMemory(sizeof(SYSTEM_RESOURCE_RAM_DISK));
    if (RamDisk == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MapRamDiskEnd;
    }

    RtlZeroMemory(RamDisk, sizeof(SYSTEM_RESOURCE_RAM_DISK));
    RamDisk->Header.Type = SystemResourceRamDisk;
    RamDisk->Header.PhysicalAddress = Base;
    RamDisk->Header.Size = Size;
    RamDisk->Header.VirtualAddress = (PVOID)-1;
    AlignedBase = ALIGN_RANGE_DOWN(Base, PageSize);
    PageOffset = Base - AlignedBase;
    AlignedSize = ALIGN_RANGE_UP(Size + PageOffset, PageSize);
    Status = BoMapPhysicalAddress(&(RamDisk->Header.VirtualAddress),
                                  AlignedBase,
                                  AlignedSize,
                                  MAP_FLAG_GLOBAL,
                                  MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto MapRamDiskEnd;
    }

    RamDisk->Header.VirtualAddress += PageOffset;

    //
    // Mark the pages of the RAM disk as loader permanent.
    //

    MmMdInitDescriptor(&Descriptor,
                       AlignedBase,
                       AlignedBase + AlignedSize,
                       MemoryTypeLoaderPermanent);

    Status = MmMdAddDescriptorToList(&BoMemoryMap, &Descriptor);
    if (!KSUCCESS(Status)) {
        goto MapRamDiskEnd;
    }

    INSERT_BEFORE(&(RamDisk->Header.ListEntry),
                  &(Parameters->SystemResourceListHead));

    Status = STATUS_SUCCESS;

MapRamDiskEnd:
    if (!KSUCCESS(Status)) {
        if (RamDisk != NULL) {
            if (RamDisk->Header.VirtualAddress != NULL) {
                BoUnmapPhysicalAddress(RamDisk->Header.VirtualAddress,
                                       AlignedSize / PageSize);
            }

            BoFreeMemory(RamDisk);
        }
    }

    return Status;
}


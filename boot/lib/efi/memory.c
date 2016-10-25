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
#include "bootlib.h"
#include "efisup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of extra descriptors to give EFI despite what they
// reported.
//

#define EFI_EXTRA_DESCRIPTOR_COUNT 10

//
// Define the number of descriptors the loader is probably going to create.
//

#define EFI_LOADER_DESCRIPTOR_ESTIMATE 50

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
BopEfiGetMemoryMap (
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
    );

EFI_STATUS
BopEfiAllocatePages (
    EFI_ALLOCATE_TYPE Type,
    EFI_MEMORY_TYPE MemoryType,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Memory
    );

EFI_STATUS
BopEfiFreePages (
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN Pages
    );

EFI_STATUS
BopEfiSetVirtualAddressMap (
    UINTN MemoryMapSize,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap
    );

VOID
BopEfiDestroyDescriptorIterator (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

MEMORY_TYPE
BopEfiConvertFromEfiMemoryType (
    EFI_MEMORY_TYPE EfiMemoryType
    );

BOOL
BopEfiDoMemoryTypesAgree (
    EFI_MEMORY_TYPE EfiType,
    MEMORY_TYPE MemoryType
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the allocation containing the memory descriptors for the memory map.
// This is the first allocation to arrive and the last to go, as it contains
// the list of other allocations to clean up.
//

EFI_PHYSICAL_ADDRESS BoEfiDescriptorAllocation;
UINTN BoEfiDescriptorAllocationPageCount;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BopEfiInitializeMemory (
    VOID
    )

/*++

Routine Description:

    This routine initializes memory services for the boot loader.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    MEMORY_DESCRIPTOR Descriptor;
    UINTN DescriptorCount;
    UINTN DescriptorIndex;
    EFI_MEMORY_DESCRIPTOR *EfiDescriptor;
    UINTN EfiDescriptorSize;
    UINT32 EfiDescriptorVersion;
    EFI_MEMORY_DESCRIPTOR *EfiMap;
    EFI_PHYSICAL_ADDRESS EfiMapAllocation;
    UINTN EfiMapKey;
    UINTN EfiMapSize;
    UINTN EfiPageCount;
    EFI_STATUS EfiStatus;
    EFI_PHYSICAL_ADDRESS LoaderDescriptorAllocation;
    UINTN LoaderDescriptorPageCount;
    UINTN LoaderDescriptorSize;
    KSTATUS Status;

    EfiMapAllocation = INVALID_PHYSICAL_ADDRESS;
    EfiMapSize = 0;
    EfiPageCount = 0;
    LoaderDescriptorAllocation = INVALID_PHYSICAL_ADDRESS;
    LoaderDescriptorPageCount = 0;
    MmMdInitDescriptorList(&BoMemoryMap, MdlAllocationSourceNone);

    //
    // Get the memory map size.
    //

    BopEfiGetMemoryMap(&EfiMapSize,
                       NULL,
                       &EfiMapKey,
                       &EfiDescriptorSize,
                       &EfiDescriptorVersion);

    if ((EfiDescriptorSize < sizeof(EFI_MEMORY_DESCRIPTOR)) ||
        (EfiMapSize == 0)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto EfiInitializeMemoryEnd;
    }

    //
    // Allocate enough pages to hold the entire set of EFI memory map
    // descriptors.
    //

    EfiMapSize += EFI_EXTRA_DESCRIPTOR_COUNT * EfiDescriptorSize;
    EfiMapSize = ALIGN_RANGE_UP(EfiMapSize, EFI_PAGE_SIZE);
    EfiPageCount = EfiMapSize >> EFI_PAGE_SHIFT;
    EfiMapAllocation = MAX_UINTN;
    EfiStatus = BopEfiAllocatePages(AllocateMaxAddress,
                                    EfiLoaderData,
                                    EfiPageCount,
                                    &EfiMapAllocation);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiInitializeMemoryEnd;
    }

    ASSERT((UINTN)EfiMapAllocation == EfiMapAllocation);

    EfiMap = (PVOID)(UINTN)EfiMapAllocation;

    //
    // Also allocate enough pages to create memory descriptors for each of
    // EFI's memory descriptors, and then some.
    //

    DescriptorCount = (EfiMapSize / EfiDescriptorSize) +
                      EFI_LOADER_DESCRIPTOR_ESTIMATE;

    LoaderDescriptorSize = DescriptorCount * sizeof(MEMORY_DESCRIPTOR);
    LoaderDescriptorSize = ALIGN_RANGE_UP(LoaderDescriptorSize, EFI_PAGE_SIZE);
    LoaderDescriptorPageCount = LoaderDescriptorSize >> EFI_PAGE_SHIFT;
    LoaderDescriptorAllocation = MAX_UINTN;
    EfiStatus = BopEfiAllocatePages(AllocateMaxAddress,
                                    EfiLoaderData,
                                    LoaderDescriptorPageCount,
                                    &LoaderDescriptorAllocation);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiInitializeMemoryEnd;
    }

    //
    // Add these descriptors to the boot memory map so it has enough to
    // contain the whole memory map.
    //

    ASSERT((UINTN)LoaderDescriptorAllocation == LoaderDescriptorAllocation);

    MmMdAddFreeDescriptorsToMdl(
                         &BoMemoryMap,
                         (PMEMORY_DESCRIPTOR)(UINTN)LoaderDescriptorAllocation,
                         LoaderDescriptorSize);

    BoEfiDescriptorAllocation = LoaderDescriptorAllocation;
    BoEfiDescriptorAllocationPageCount = LoaderDescriptorPageCount;
    LoaderDescriptorAllocation = INVALID_PHYSICAL_ADDRESS;

    //
    // Now get the memory map for real this time.
    //

    EfiStatus = BopEfiGetMemoryMap(&EfiMapSize,
                                   EfiMap,
                                   &EfiMapKey,
                                   &EfiDescriptorSize,
                                   &EfiDescriptorVersion);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiInitializeMemoryEnd;
    }

    //
    // Loop through the descriptors, matching descriptors.
    //

    ASSERT(BoMemoryMap.DescriptorCount == 0);

    DescriptorIndex = 0;
    while ((DescriptorIndex + 1) * EfiDescriptorSize <= EfiMapSize) {
        EfiDescriptor = (EFI_MEMORY_DESCRIPTOR *)((PVOID)EfiMap +
                                        (DescriptorIndex * EfiDescriptorSize));

        DescriptorIndex += 1;
        Descriptor.BaseAddress = EfiDescriptor->PhysicalStart;
        Descriptor.Size = EfiDescriptor->NumberOfPages << EFI_PAGE_SHIFT;
        Descriptor.Type = BopEfiConvertFromEfiMemoryType(EfiDescriptor->Type);
        Descriptor.Flags = 0;
        Status = MmMdAddDescriptorToList(&BoMemoryMap, &Descriptor);
        if (!KSUCCESS(Status)) {
            goto EfiInitializeMemoryEnd;
        }
    }

    Status = STATUS_SUCCESS;

EfiInitializeMemoryEnd:
    if (LoaderDescriptorAllocation != INVALID_PHYSICAL_ADDRESS) {
        BopEfiFreePages(LoaderDescriptorAllocation, LoaderDescriptorPageCount);
    }

    if (EfiMapAllocation != INVALID_PHYSICAL_ADDRESS) {
        BopEfiFreePages(EfiMapAllocation, EfiPageCount);
    }

    return Status;
}

VOID
BopEfiDestroyMemory (
    VOID
    )

/*++

Routine Description:

    This routine cleans up memory services upon failure.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // If the memory subsystem was never initialized, there's nothing to do.
    //

    if (BoEfiDescriptorAllocationPageCount == 0) {
        return;
    }

    //
    // This function makes some assumptions about page sizes.
    //

    ASSERT(MmPageSize() == EFI_PAGE_SIZE);

    //
    // Loop through every descriptor in the memory map, and free any that were
    // allocated by the loader.
    //

    MmMdIterate(&BoMemoryMap, BopEfiDestroyDescriptorIterator, NULL);

    //
    // Finally, free the allocation that hold the memory descriptors.
    //

    BopEfiFreePages(BoEfiDescriptorAllocation,
                    BoEfiDescriptorAllocationPageCount);

    BoEfiDescriptorAllocation = INVALID_PHYSICAL_ADDRESS;
    BoEfiDescriptorAllocationPageCount = 0;
    return;
}

KSTATUS
BopEfiLoaderAllocatePages (
    PULONGLONG Address,
    ULONGLONG Size,
    MEMORY_TYPE MemoryType
    )

/*++

Routine Description:

    This routine allocates physical pages for use.

Arguments:

    Address - Supplies a pointer to where the allocation will be returned.

    Size - Supplies the size of the required space, in bytes.

    MemoryType - Supplies the type of memory to mark the allocation as.

Return Value:

    STATUS_SUCCESS if the allocation was successful.

    STATUS_INVALID_PARAMETER if a page count of 0 was passed or the address
        parameter was not filled out.

    STATUS_NO_MEMORY if the allocation request could not be filled.

--*/

{

    EFI_PHYSICAL_ADDRESS Allocation;
    MEMORY_DESCRIPTOR Descriptor;
    EFI_STATUS EfiStatus;
    UINTN PageCount;
    KSTATUS Status;

    //
    // This will need to be handled on migration to an architecture with
    // differently sized pages.
    //

    ASSERT(MmPageSize() == EFI_PAGE_SIZE);

    //
    // More asserts that need to be handled if they come up.
    //

    ASSERT((Size & EFI_PAGE_MASK) == 0);

    PageCount = Size >> EFI_PAGE_SHIFT;
    Allocation = MAX_UINTN;
    EfiStatus = BopEfiAllocatePages(AllocateMaxAddress,
                                    EfiLoaderData,
                                    PageCount,
                                    &Allocation);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiLoaderAllocatePagesEnd;
    }

    //
    // This assert is here to remind everyone that if the loader exits in
    // error, it's responsible for freeing all of its allocations. The code in
    // destroy knows to look for these types in the MDL to free. If folks in
    // the loader are allocating other types they'll need to be dealt with
    // there. It's important that those types of allocations not be confused
    // with any that might come from the initial EFI memory map, otherwise the
    // destroy routine won't be able to know what to free.
    //

    ASSERT((MemoryType == MemoryTypePageTables) ||
           (MemoryType == MemoryTypeBootPageTables) ||
           (MemoryType == MemoryTypeLoaderTemporary) ||
           (MemoryType == MemoryTypeLoaderPermanent));

    //
    // Also add the descriptor to the list.
    //

    RtlZeroMemory(&Descriptor, sizeof(MEMORY_DESCRIPTOR));
    Descriptor.BaseAddress = Allocation;
    Descriptor.Size = PageCount << EFI_PAGE_SHIFT;
    Descriptor.Type = MemoryType;
    Status = MmMdAddDescriptorToList(&BoMemoryMap, &Descriptor);
    if (!KSUCCESS(Status)) {
        goto EfiLoaderAllocatePagesEnd;
    }

    Status = STATUS_SUCCESS;

EfiLoaderAllocatePagesEnd:
    *Address = Allocation;
    return Status;
}

KSTATUS
BopEfiSynchronizeMemoryMap (
    PUINTN Key
    )

/*++

Routine Description:

    This routine synchronizes the EFI memory map with the boot memory map.

Arguments:

    Key - Supplies a pointer where the latest EFI memory map key will be
        returned.

Return Value:

    Status code.

--*/

{

    BOOL Agree;
    ULONGLONG CurrentBase;
    PMEMORY_DESCRIPTOR Descriptor;
    ULONGLONG DescriptorEnd;
    UINTN DescriptorIndex;
    EFI_MEMORY_DESCRIPTOR *EfiDescriptor;
    UINTN EfiDescriptorSize;
    UINT32 EfiDescriptorVersion;
    EFI_MEMORY_DESCRIPTOR *EfiMap;
    UINTN EfiMapKey;
    UINTN EfiMapSize;
    BOOL Failed;
    MEMORY_DESCRIPTOR NewDescriptor;
    KSTATUS Status;

    EfiMap = NULL;
    EfiMapKey = 0;
    RtlZeroMemory(&NewDescriptor, sizeof(MEMORY_DESCRIPTOR));
    Status = BopEfiGetAllocatedMemoryMap(&EfiMapSize,
                                         &EfiMap,
                                         &EfiMapKey,
                                         &EfiDescriptorSize,
                                         &EfiDescriptorVersion);

    if (!KSUCCESS(Status)) {
        goto EfiSynchronizeMemoryMapEnd;
    }

    //
    // Loop over each EFI memory descriptor.
    //

    Failed = FALSE;
    DescriptorIndex = 0;
    while ((DescriptorIndex + 1) * EfiDescriptorSize <= EfiMapSize) {
        EfiDescriptor = (EFI_MEMORY_DESCRIPTOR *)((PVOID)EfiMap +
                                        (DescriptorIndex * EfiDescriptorSize));

        DescriptorIndex += 1;

        //
        // Loop until the entire EFI descriptor is covered by a boot descriptor.
        //

        CurrentBase = EfiDescriptor->PhysicalStart;
        DescriptorEnd = CurrentBase +
                        (EfiDescriptor->NumberOfPages << EFI_PAGE_SHIFT);

        while (CurrentBase < DescriptorEnd) {
            Descriptor = MmMdLookupDescriptor(&BoMemoryMap,
                                              CurrentBase,
                                              CurrentBase + 1);

            //
            // Add the descriptor to the OS list under any of the following
            // conditions:
            // 1) There is no descriptor there.
            // 2) The loader thinks it's free but the firmware says it's not.
            // 3) The firmware says it's free but the loader thought it wasn't.
            //

            if ((Descriptor == NULL) ||
                ((Descriptor->Type == MemoryTypeFree) &&
                 (EfiDescriptor->Type != EfiConventionalMemory)) ||
                ((EfiDescriptor->Type == EfiConventionalMemory) &&
                 (Descriptor->Type != MemoryTypeFree))) {

                //
                // Assert that if the firmware thinks it's free, the loader
                // must have had it marked as firmware temporary.
                //

                ASSERT((EfiDescriptor->Type != EfiConventionalMemory) ||
                       (Descriptor->Type == MemoryTypeFirmwareTemporary));

                NewDescriptor.Type =
                           BopEfiConvertFromEfiMemoryType(EfiDescriptor->Type);

                NewDescriptor.BaseAddress = CurrentBase;
                NewDescriptor.Size = DescriptorEnd - CurrentBase;
                Status = MmMdAddDescriptorToList(&BoMemoryMap,
                                                 &NewDescriptor);

                if (!KSUCCESS(Status)) {
                    RtlDebugPrint("Failed to add memory descriptor type %d, "
                                  "0x%I64x - 0x%I64x: Status %d\n",
                                  NewDescriptor.Type,
                                  NewDescriptor.BaseAddress,
                                  NewDescriptor.Size,
                                  Status);

                    goto EfiSynchronizeMemoryMapEnd;
                }

                CurrentBase = DescriptorEnd;

            //
            // If there is something there, verify it agrees with the boot
            // descriptor.
            //

            } else {
                Agree = BopEfiDoMemoryTypesAgree(EfiDescriptor->Type,
                                                 Descriptor->Type);

                if (Agree == FALSE) {
                    RtlDebugPrint("Error: Memory conflict!\nEFI Descriptor "
                                  "type %d, PA 0x%I64x, %I64d pages, 0x%I64x.\n"
                                  "Boot Descriptor type %d, PA 0x%I64x, size "
                                  "0x%I64x.\n",
                                  EfiDescriptor->Type,
                                  EfiDescriptor->PhysicalStart,
                                  EfiDescriptor->NumberOfPages,
                                  EfiDescriptor->Attribute,
                                  Descriptor->Type,
                                  Descriptor->BaseAddress,
                                  Descriptor->Size);

                    Failed = TRUE;
                }

                CurrentBase = Descriptor->BaseAddress + Descriptor->Size;
            }
        }
    }

    if (Failed != FALSE) {
        Status = STATUS_MEMORY_CONFLICT;
        goto EfiSynchronizeMemoryMapEnd;
    }

    Status = STATUS_SUCCESS;

EfiSynchronizeMemoryMapEnd:
    if (EfiMap != NULL) {
        BoFreeMemory(EfiMap);
    }

    *Key = EfiMapKey;
    return Status;
}

KSTATUS
BopEfiVirtualizeFirmwareServices (
    UINTN MemoryMapSize,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap
    )

/*++

Routine Description:

    This routine changes the runtime addressing mode of EFI firmware from
    physical to virtual.

Arguments:

    MemoryMapSize - Supplies the size of the virtual map.

    DescriptorSize - Supplies the size of an entry in the virtual map.

    DescriptorVersion - Supplies the version of the structure entries in the
        virtual map.

    VirtualMap - Supplies the array of memory descriptors which contain the
        new virtual address mappings for all runtime ranges.

Return Value:

    Status code.

--*/

{

    EFI_STATUS EfiStatus;
    KSTATUS Status;

    EfiStatus = BopEfiSetVirtualAddressMap(MemoryMapSize,
                                           DescriptorSize,
                                           DescriptorVersion,
                                           VirtualMap);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiVirtualizeFirmwareServicesEnd;
    }

    BoEfiRuntimeServices = BoEfiSystemTable->RuntimeServices;
    Status = STATUS_SUCCESS;

EfiVirtualizeFirmwareServicesEnd:
    return Status;
}

KSTATUS
BopEfiGetAllocatedMemoryMap (
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR **MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
    )

/*++

Routine Description:

    This routine returns the current memory map.

Arguments:

    MemoryMapSize - Supplies a pointer where the size in bytes of the map
        buffer will be returned.

    MemoryMap - Supplies a pointer where a pointer to the memory map will be
        returned on success. The caller is responsible for freeing this memory.

    MapKey - Supplies a pointer where the map key will be returned on success.

    DescriptorSize - Supplies a pointer where the firmware returns the size of
        the EFI_MEMORY_DESCRIPTOR structure.

    DescriptorVersion - Supplies a pointer where the firmware returns the
        version number associated with the EFI_MEMORY_DESCRIPTOR structure.

Return Value:

    Status code.

--*/

{

    EFI_MEMORY_DESCRIPTOR *EfiMap;
    UINTN EfiMapSize;
    EFI_STATUS EfiStatus;
    KSTATUS Status;

    EfiMap = NULL;

    //
    // Get the memory map size.
    //

    EfiMapSize = 0;
    BopEfiGetMemoryMap(&EfiMapSize,
                       NULL,
                       MapKey,
                       DescriptorSize,
                       DescriptorVersion);

    if ((*DescriptorSize < sizeof(EFI_MEMORY_DESCRIPTOR)) ||
        (EfiMapSize == 0)) {

        Status = STATUS_INVALID_CONFIGURATION;
        goto EfiGetEfiMemoryMap;
    }

    //
    // Allocate enough pages to hold the entire set of EFI memory map
    // descriptors.
    //

    EfiMapSize += EFI_EXTRA_DESCRIPTOR_COUNT * *DescriptorSize;
    EfiMap = BoAllocateMemory(EfiMapSize);
    if (EfiMap == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EfiGetEfiMemoryMap;
    }

    EfiStatus = BopEfiGetMemoryMap(&EfiMapSize,
                                   EfiMap,
                                   MapKey,
                                   DescriptorSize,
                                   DescriptorVersion);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiGetEfiMemoryMap;
    }

    *MemoryMapSize = EfiMapSize;
    Status = STATUS_SUCCESS;

EfiGetEfiMemoryMap:
    if (!KSUCCESS(Status)) {
        if (EfiMap != NULL) {
            BoFreeMemory(EfiMap);
            EfiMap = NULL;
        }
    }

    *MemoryMap = EfiMap;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
BopEfiGetMemoryMap (
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
    )

/*++

Routine Description:

    This routine returns the current memory map.

Arguments:

    MemoryMapSize - Supplies a pointer to the size, in bytes, of the memory
        map buffer. On input, this is the size of the buffer allocated by the
        caller. On output, this is the size of the buffer returned by the
        firmware if the buffer was large enough, or the size of the buffer
        needed if the buffer was too small.

    MemoryMap - Supplies a pointer to a caller-allocated buffer where the
        memory map will be written on success.

    MapKey - Supplies a pointer where the firmware returns the map key.

    DescriptorSize - Supplies a pointer where the firmware returns the size of
        the EFI_MEMORY_DESCRIPTOR structure.

    DescriptorVersion - Supplies a pointer where the firmware returns the
        version number associated with the EFI_MEMORY_DESCRIPTOR structure.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->GetMemoryMap(MemoryMapSize,
                                             MemoryMap,
                                             MapKey,
                                             DescriptorSize,
                                             DescriptorVersion);

    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiAllocatePages (
    EFI_ALLOCATE_TYPE Type,
    EFI_MEMORY_TYPE MemoryType,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Memory
    )

/*++

Routine Description:

    This routine allocates memory pages from the firmware.

Arguments:

    Type - Supplies the allocation strategy to use.

    MemoryType - Supplies the memory type of the allocation.

    Pages - Supplies the number of contiguous EFI_PAGE_SIZE pages.

    Memory - Supplies a pointer that on input contains a physical address whose
        use depends on the allocation strategy. On output, the physical address
        of the allocation will be returned.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->AllocatePages(Type, MemoryType, Pages, Memory);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiFreePages (
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN Pages
    )

/*++

Routine Description:

    This routine frees memory pages back to the system.

Arguments:

    Memory - Supplies the base physical address of the allocation to free.

    Pages - Supplies the number of pages to free.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiBootServices->FreePages(Memory, Pages);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiSetVirtualAddressMap (
    UINTN MemoryMapSize,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion,
    EFI_MEMORY_DESCRIPTOR *VirtualMap
    )

/*++

Routine Description:

    This routine changes the runtime addressing mode of EFI firmware from
    physical to virtual.

Arguments:

    MemoryMapSize - Supplies the size of the virtual map.

    DescriptorSize - Supplies the size of an entry in the virtual map.

    DescriptorVersion - Supplies the version of the structure entries in the
        virtual map.

    VirtualMap - Supplies the array of memory descriptors which contain the
        new virtual address mappings for all runtime ranges.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = BoEfiRuntimeServices->SetVirtualAddressMap(MemoryMapSize,
                                                        DescriptorSize,
                                                        DescriptorVersion,
                                                        VirtualMap);

    BopEfiRestoreApplicationContext();
    return Status;
}

VOID
BopEfiDestroyDescriptorIterator (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each descriptor in the memory descriptor
    list.

Arguments:

    DescriptorList - Supplies a pointer to the descriptor list being iterated
        over.

    Descriptor - Supplies a pointer to the current descriptor.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

{

    UINTN PageCount;

    //
    // Skip any regions that aren't loader allocations.
    //

    if ((Descriptor->Type != MemoryTypePageTables) &&
        (Descriptor->Type != MemoryTypeBootPageTables) &&
        (Descriptor->Type != MemoryTypeLoaderTemporary) &&
        (Descriptor->Type != MemoryTypeLoaderPermanent)) {

        return;
    }

    ASSERT((Descriptor->Size & EFI_PAGE_MASK) == 0);
    ASSERT((Descriptor->BaseAddress & EFI_PAGE_MASK) == 0);

    PageCount = Descriptor->Size >> EFI_PAGE_SHIFT;
    BopEfiFreePages(Descriptor->BaseAddress, PageCount);
    return;
}

MEMORY_TYPE
BopEfiConvertFromEfiMemoryType (
    EFI_MEMORY_TYPE EfiMemoryType
    )

/*++

Routine Description:

    This routine converts an EFI memory type into an OS memory type.

Arguments:

    EfiMemoryType - Supplies the EFI memory type.

Return Value:

    Returns a conversion of the memory type. If unknown, returns firmware
    permanent memory.

--*/

{

    MEMORY_TYPE Type;

    Type = MemoryTypeFirmwarePermanent;
    switch (EfiMemoryType) {
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
        Type = MemoryTypeFirmwareTemporary;
        break;

    case EfiRuntimeServicesCode:
    case EfiRuntimeServicesData:
        break;

    case EfiConventionalMemory:
        Type = MemoryTypeFree;
        break;

    case EfiUnusableMemory:
        Type = MemoryTypeBad;
        break;

    case EfiACPIReclaimMemory:
        Type = MemoryTypeAcpiTables;
        break;

    case EfiACPIMemoryNVS:
        Type = MemoryTypeAcpiNvStorage;
        break;

    case EfiMemoryMappedIO:
    case EfiMemoryMappedIOPortSpace:
    case EfiPalCode:
    case EfiReservedMemoryType:
    default:
        break;
    }

    return Type;
}

BOOL
BopEfiDoMemoryTypesAgree (
    EFI_MEMORY_TYPE EfiType,
    MEMORY_TYPE MemoryType
    )

/*++

Routine Description:

    This routine determines if an EFI memory type agrees with an OS memory type.

Arguments:

    EfiType - Supplies the EFI memory type.

    MemoryType - Supplies the OS memory type.

Return Value:

    TRUE if the OS memory type appropriately describes the EFI memory type.

    FALSE otherwise.

--*/

{

    BOOL Agree;

    Agree = FALSE;
    switch (EfiType) {
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
        switch (MemoryType) {
        case MemoryTypeFirmwareTemporary:
        case MemoryTypeLoaderTemporary:
        case MemoryTypeLoaderPermanent:
        case MemoryTypePageTables:
        case MemoryTypeBootPageTables:
            Agree = TRUE;
            break;

        default:
            break;
        }

        break;

    case EfiRuntimeServicesCode:
    case EfiRuntimeServicesData:
        if ((MemoryType == MemoryTypeFirmwarePermanent) ||
            (MemoryType == MemoryTypeAcpiTables)) {

            Agree = TRUE;
        }

        break;

    case EfiConventionalMemory:
        if (MemoryType == MemoryTypeFree) {
            Agree = TRUE;
        }

        break;

    case EfiUnusableMemory:
        if (MemoryType == MemoryTypeBad) {
            Agree = TRUE;
        }

        break;

    case EfiACPIReclaimMemory:
        if (MemoryType == MemoryTypeAcpiTables) {
            Agree = TRUE;
        }

        break;

    case EfiACPIMemoryNVS:
        if (MemoryType == MemoryTypeAcpiNvStorage) {
            Agree = TRUE;
        }

        break;

    case EfiMemoryMappedIO:
    case EfiMemoryMappedIOPortSpace:
    case EfiPalCode:
    case EfiReservedMemoryType:
    default:
        if (MemoryType == MemoryTypeFirmwarePermanent) {
            Agree = TRUE;
        }

        break;
    }

    return Agree;
}


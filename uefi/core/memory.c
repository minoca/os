/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    memory.c

Abstract:

    This module implements core UEFI memory map services.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_DEFAULT_PAGE_ALLOCATION_ALIGNMENT EFI_PAGE_SIZE
#define EFI_ACPI_RUNTIME_PAGE_ALLOCATION_ALIGNMENT EFI_PAGE_SIZE

//
// Define the maximum number of temporary descriptors that will ever be
// needed simultaneously.
//

#define EFI_DESCRIPTOR_STACK_SIZE 6

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_MEMORY_TYPE_STATISTICS {
    EFI_PHYSICAL_ADDRESS BaseAddress;
    EFI_PHYSICAL_ADDRESS MaximumAddress;
    UINT64 CurrentPageCount;
    UINT64 PageCount;
    UINTN InformationIndex;
    BOOLEAN Special;
    BOOLEAN Runtime;
} EFI_MEMORY_TYPE_STATISTICS, *PEFI_MEMORY_TYPE_STATISTICS;

typedef struct _EFI_MEMORY_TYPE_INFORMATION {
    UINT32 Type;
    UINT32 PageCount;
} EFI_MEMORY_TYPE_INFORMATION, *PEFI_MEMORY_TYPE_INFORMATION;

typedef struct _EFI_MEMORY_MAP_ENTRY {
    LIST_ENTRY ListEntry;
    BOOL Temporary;
    EFI_MEMORY_DESCRIPTOR Descriptor;
} EFI_MEMORY_MAP_ENTRY, *PEFI_MEMORY_MAP_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipCoreAddMemoryDescriptor (
    EFI_MEMORY_TYPE Type,
    EFI_PHYSICAL_ADDRESS Start,
    UINT64 PageCount,
    UINT64 Attribute
    );

UINT64
EfipCoreFindFreePages (
    UINT64 MaxAddress,
    UINT64 PageCount,
    EFI_MEMORY_TYPE NewType,
    UINTN Alignment
    );

UINT64
EfipCoreFindFreePagesInRange (
    UINT64 MaxAddress,
    UINT64 MinAddress,
    UINT64 PageCount,
    EFI_MEMORY_TYPE NewType,
    UINTN Alignment
    );

EFI_STATUS
EfipCoreConvertPages (
    UINT64 Start,
    UINT64 PageCount,
    EFI_MEMORY_TYPE NewType
    );

VOID
EfipCoreAddRange (
    EFI_MEMORY_TYPE Type,
    EFI_PHYSICAL_ADDRESS Start,
    EFI_PHYSICAL_ADDRESS End,
    UINT64 Attribute
    );

EFI_MEMORY_DESCRIPTOR *
EfipCoreMergeMemoryMapDescriptor (
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    EFI_MEMORY_DESCRIPTOR *LastDescriptor,
    UINTN DescriptorSize
    );

VOID
EfipCoreRemoveMemoryMapEntry (
    PEFI_MEMORY_MAP_ENTRY Entry
    );

VOID
EfipCoreFlushMemoryMapStack (
    VOID
    );

PEFI_MEMORY_MAP_ENTRY
EfipCoreAllocateMemoryMapEntry (
    VOID
    );

VOID
EfipDebugPrintMemoryMap (
    EFI_MEMORY_DESCRIPTOR *Map,
    UINTN MapSize,
    UINTN DescriptorSize
    );

VOID
EfipDebugPrintMemoryDescriptor (
    EFI_MEMORY_DESCRIPTOR *Descriptor
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the memory "lock", which really just helps prevent re-entering
// functions in a notify ISR.
//

EFI_LOCK EfiMemoryLock;

//
// Store the memory map itself, which is just a list of EFI_MEMORY_MAP_ENTRY
// structures.
//

LIST_ENTRY EfiMemoryMap;

//
// Store the memory map key, essentially a sequence number on the memory map.
//

UINTN EfiMemoryMapKey;

//
// Store a list of free descriptors to use.
//

LIST_ENTRY EfiFreeDescriptorList;

//
// Define the default memory range to search.
//

EFI_PHYSICAL_ADDRESS EfiDefaultMaximumAddress = MAX_ADDRESS;
EFI_PHYSICAL_ADDRESS EfiDefaultBaseAddress = MAX_ADDRESS;

//
// Define the stack of temporary descriptors used during operations.
//

UINTN EfiDescriptorStackSize = 0;
EFI_MEMORY_MAP_ENTRY EfiDescriptorStack[EFI_DESCRIPTOR_STACK_SIZE];
BOOLEAN EfiDescriptorStackFreeInProgress = FALSE;

//
// Store memory statistics, which help cluster allocations of the same type
// together.
//

BOOLEAN EfiMemoryTypeInformationInitialized = FALSE;

EFI_MEMORY_TYPE_STATISTICS EfiMemoryStatistics[EfiMaxMemoryType + 1] = {
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, TRUE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, TRUE, TRUE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, TRUE, TRUE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, TRUE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, TRUE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, TRUE, TRUE},
    {0, MAX_ADDRESS, 0, 0, EfiMaxMemoryType, FALSE, FALSE}
};

EFI_MEMORY_TYPE_INFORMATION EfiMemoryTypeInformation[EfiMaxMemoryType + 1] = {
    {EfiReservedMemoryType, 0},
    {EfiLoaderCode, 0},
    {EfiLoaderData, 0},
    {EfiBootServicesCode, 0},
    {EfiBootServicesData, 0},
    {EfiRuntimeServicesCode, 0},
    {EfiRuntimeServicesData, 0},
    {EfiConventionalMemory, 0},
    {EfiUnusableMemory, 0},
    {EfiACPIReclaimMemory, 0},
    {EfiACPIMemoryNVS, 0},
    {EfiMemoryMappedIO, 0},
    {EfiMemoryMappedIOPortSpace, 0},
    {EfiPalCode, 0},
    {EfiMaxMemoryType, 0}
};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreAllocatePages (
    EFI_ALLOCATE_TYPE Type,
    EFI_MEMORY_TYPE MemoryType,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Memory
    )

/*++

Routine Description:

    This routine allocates memory pages from the system.

Arguments:

    Type - Supplies the allocation strategy to use.

    MemoryType - Supplies the memory type of the allocation.

    Pages - Supplies the number of contiguous EFI_PAGE_SIZE pages.

    Memory - Supplies a pointer that on input contains a physical address whose
        use depends on the allocation strategy. On output, the physical address
        of the allocation will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the Type or MemoryType are invalid, or Memory is
    NULL.

    EFI_OUT_OF_RESOURCES if the pages could not be allocated.

    EFI_NOT_FOUND if the requested pages could not be found.

--*/

{

    UINTN Alignment;
    UINT64 MaxAddress;
    UINT64 Start;
    EFI_STATUS Status;

    if (Type >= MaxAllocateType) {
        return EFI_INVALID_PARAMETER;
    }

    if ((((UINT32)MemoryType >= EfiMaxMemoryType) &&
        ((UINT32)MemoryType < 0x7FFFFFFF)) ||
        (MemoryType == EfiConventionalMemory)) {

        return EFI_INVALID_PARAMETER;
    }

    if (Memory == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Alignment = EFI_DEFAULT_PAGE_ALLOCATION_ALIGNMENT;
    if ((MemoryType == EfiACPIReclaimMemory) ||
        (MemoryType == EfiACPIMemoryNVS) ||
        (MemoryType == EfiRuntimeServicesCode) ||
        (MemoryType == EfiRuntimeServicesData)) {

        Alignment = EFI_ACPI_RUNTIME_PAGE_ALLOCATION_ALIGNMENT;
    }

    if (Type == AllocateAddress) {
        if ((*Memory & (Alignment - 1)) != 0) {
            return EFI_NOT_FOUND;
        }
    }

    Pages += EFI_SIZE_TO_PAGES(Alignment) - 1;
    Pages &= ~(EFI_SIZE_TO_PAGES(Alignment) - 1);
    Start = *Memory;
    MaxAddress = MAX_ADDRESS;
    if (Type == AllocateMaxAddress) {
        MaxAddress = Start;
    }

    EfiCoreAcquireLock(&EfiMemoryLock);

    //
    // If no specific address was requested, then locate some pages.
    //

    if (Type != AllocateAddress) {
        Start = EfipCoreFindFreePages(MaxAddress, Pages, MemoryType, Alignment);
        if (Start == 0) {
            Status = EFI_OUT_OF_RESOURCES;
            goto CoreAllocatePagesEnd;
        }
    }

    //
    // Mark the pages as allocated.
    //

    Status = EfipCoreConvertPages(Start, Pages, MemoryType);

CoreAllocatePagesEnd:
    EfiCoreReleaseLock(&EfiMemoryLock);
    if (!EFI_ERROR(Status)) {
        *Memory = Start;
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreFreePages (
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

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the memory is not page aligned or is invalid.

    EFI_NOT_FOUND if the requested pages were not allocated.

--*/

{

    UINTN Alignment;
    PLIST_ENTRY CurrentEntry;
    PEFI_MEMORY_MAP_ENTRY Entry;
    UINT64 EntryEnd;
    UINT64 EntryStart;
    EFI_STATUS Status;

    EfiCoreAcquireLock(&EfiMemoryLock);
    Entry = NULL;
    CurrentEntry = EfiMemoryMap.Next;
    while (CurrentEntry != &EfiMemoryMap) {
        Entry = LIST_VALUE(CurrentEntry, EFI_MEMORY_MAP_ENTRY, ListEntry);
        EntryStart = Entry->Descriptor.PhysicalStart;
        EntryEnd = EntryStart +
                   (Entry->Descriptor.NumberOfPages << EFI_PAGE_SHIFT) - 1;

        if ((EntryStart <= Memory) && (EntryEnd > Memory)) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &EfiMemoryMap) {
        Status = EFI_NOT_FOUND;
        goto CoreFreePagesEnd;
    }

    Alignment = EFI_DEFAULT_PAGE_ALLOCATION_ALIGNMENT;

    ASSERT(Entry != NULL);

    if ((Entry->Descriptor.Type == EfiACPIReclaimMemory) ||
        (Entry->Descriptor.Type == EfiACPIMemoryNVS) ||
        (Entry->Descriptor.Type == EfiRuntimeServicesCode) ||
        (Entry->Descriptor.Type == EfiRuntimeServicesData)) {

        Alignment = EFI_ACPI_RUNTIME_PAGE_ALLOCATION_ALIGNMENT;
    }

    if ((Memory & (Alignment - 1)) != 0) {
        Status = EFI_INVALID_PARAMETER;
        goto CoreFreePagesEnd;
    }

    Pages += EFI_SIZE_TO_PAGES(Alignment) - 1;
    Pages &= ~(EFI_SIZE_TO_PAGES(Alignment) - 1);
    Status = EfipCoreConvertPages(Memory, Pages, EfiConventionalMemory);
    if (EFI_ERROR(Status)) {
        goto CoreFreePagesEnd;
    }

CoreFreePagesEnd:
    EfiCoreReleaseLock(&EfiMemoryLock);
    return Status;
}

EFIAPI
EFI_STATUS
EfiCoreGetMemoryMap (
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

    EFI_SUCCESS on success.

    EFI_BUFFER_TOO_SMALL if the supplied buffer was too small. The size needed
    is returned in the size parameter.

    EFI_INVALID_PARAMETER if the supplied size or memory map pointers are NULL.

--*/

{

    UINTN BufferSize;
    PLIST_ENTRY CurrentEntry;
    PEFI_MEMORY_MAP_ENTRY Entry;
    UINT64 EntryEnd;
    UINT64 EntryStart;
    EFI_MEMORY_DESCRIPTOR *MemoryMapStart;
    UINTN Size;
    EFI_STATUS Status;
    EFI_MEMORY_TYPE Type;

    if (MemoryMapSize == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Size = sizeof(EFI_MEMORY_DESCRIPTOR);

    //
    // Artificially change the reported size to foil folks using pointer
    // arithmetic. This forces them to use the returned descriptor size.
    //

    Size += sizeof(UINT64) - (Size % sizeof(UINT64));
    if (DescriptorSize != NULL) {
        *DescriptorSize = Size;
    }

    if (DescriptorVersion != NULL) {
        *DescriptorVersion = EFI_MEMORY_DESCRIPTOR_VERSION;
    }

    EfiCoreAcquireLock(&EfiMemoryLock);

    //
    // Compute the size required to contain the entire map.
    //

    BufferSize = 0;
    CurrentEntry = EfiMemoryMap.Next;
    while (CurrentEntry != &EfiMemoryMap) {
        Entry = LIST_VALUE(CurrentEntry, EFI_MEMORY_MAP_ENTRY, ListEntry);
        BufferSize += Size;
        CurrentEntry = CurrentEntry->Next;
    }

    if (*MemoryMapSize < BufferSize) {
        Status = EFI_BUFFER_TOO_SMALL;
        goto CoreGetMemoryMapEnd;
    }

    if (MemoryMap == NULL) {
        Status = EFI_INVALID_PARAMETER;
        goto CoreGetMemoryMapEnd;
    }

    //
    // Build the memory map.
    //

    EfiCoreSetMemory(MemoryMap, BufferSize, 0);
    MemoryMapStart = MemoryMap;
    CurrentEntry = EfiMemoryMap.Next;
    while (CurrentEntry != &EfiMemoryMap) {
        Entry = LIST_VALUE(CurrentEntry, EFI_MEMORY_MAP_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(Entry->Descriptor.VirtualStart == 0);

        EfiCoreCopyMemory(MemoryMap,
                          &(Entry->Descriptor),
                          sizeof(EFI_MEMORY_DESCRIPTOR));

        //
        // If the memory type is free memory, then determine if the range is
        // part of a memory type bin and needs to be converted to the same
        // memory type as the rest of the memory type bin in order to minimize
        // EFI memory map changes across reboots. This improves the chances for
        // a successful S4 resume in the presence of minor page allocation
        // differences across reboots.
        //

        if (MemoryMap->Type == EfiConventionalMemory) {
            EntryStart = Entry->Descriptor.PhysicalStart;
            EntryEnd = EntryStart +
                       (Entry->Descriptor.NumberOfPages << EFI_PAGE_SHIFT) - 1;

            for (Type = 0; Type < EfiMaxMemoryType; Type += 1) {
                if ((EfiMemoryStatistics[Type].Special != FALSE) &&
                    (EfiMemoryStatistics[Type].PageCount > 0) &&
                    (EntryStart >= EfiMemoryStatistics[Type].BaseAddress) &&
                    (EntryEnd <= EfiMemoryStatistics[Type].MaximumAddress)) {

                    MemoryMap->Type = Type;
                }
            }
        }

        if ((MemoryMap->Type < EfiMaxMemoryType) &&
            (EfiMemoryStatistics[MemoryMap->Type].Runtime != FALSE)) {

            MemoryMap->Attribute |= EFI_MEMORY_RUNTIME;
        }

        //
        // Check to see if the new memory map descriptor can be merged with an
        // existing descriptor.
        //

        MemoryMap = EfipCoreMergeMemoryMapDescriptor(MemoryMapStart,
                                                     MemoryMap,
                                                     Size);

    }

    //
    // Compute the buffer size actually used after all the merge operations.
    //

    BufferSize = (UINTN)MemoryMap - (UINTN)MemoryMapStart;
    Status = EFI_SUCCESS;

CoreGetMemoryMapEnd:
    if (MapKey != NULL) {
        *MapKey = EfiMemoryMapKey;
    }

    EfiCoreReleaseLock(&EfiMemoryLock);
    *MemoryMapSize = BufferSize;
    return Status;
}

VOID *
EfiCoreAllocatePoolPages (
    EFI_MEMORY_TYPE PoolType,
    UINTN PageCount,
    UINTN Alignment
    )

/*++

Routine Description:

    This routine allocates pages to back pool allocations and memory map
    descriptors.

Arguments:

    PoolType - Supplies the memory type of the allocation.

    PageCount - Supplies the number of pages to allocate.

    Alignment - Supplies the required alignment.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure.

--*/

{

    UINT64 Start;

    Start = EfipCoreFindFreePages(MAX_ADDRESS, PageCount, PoolType, Alignment);

    ASSERT(Start != 0);

    if (Start != 0) {
        EfipCoreConvertPages(Start, PageCount, PoolType);
    }

    return (VOID *)(UINTN)Start;
}

VOID
EfiCoreFreePoolPages (
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN PageCount
    )

/*++

Routine Description:

    This routine frees pages allocated for pool or descriptor.

Arguments:

    Memory - Supplies the address of the allocation.

    PageCount - Supplies the number of pages to free.

Return Value:

    None.

--*/

{

    EfipCoreConvertPages(Memory, PageCount, EfiConventionalMemory);
    return;
}

EFI_STATUS
EfiCoreInitializeMemoryServices (
    VOID *FirmwareLowestAddress,
    UINTN FirmwareSize,
    VOID *StackBase,
    UINTN StackSize
    )

/*++

Routine Description:

    This routine initializes core UEFI memory services.

Arguments:

    FirmwareLowestAddress - Supplies the lowest address where the firmware was
        loaded into memory.

    FirmwareSize - Supplies the size of the firmware image in memory, in bytes.

    StackBase - Supplies the base (lowest) address of the stack.

    StackSize - Supplies the size in bytes of the stack. This should be at
        least 0x4000 bytes (16kB).

Return Value:

    EFI status code.

--*/

{

    EFI_MEMORY_DESCRIPTOR *Entry;
    EFI_PHYSICAL_ADDRESS EntryAddress;
    EFI_MEMORY_DESCRIPTOR *FreeEntry;
    UINTN Index;
    EFI_MEMORY_DESCRIPTOR *PlatformMap;
    UINTN PlatformMapSize;
    EFI_STATUS Status;

    EfiCoreInitializeLock(&EfiMemoryLock, TPL_NOTIFY);
    INITIALIZE_LIST_HEAD(&EfiMemoryMap);
    INITIALIZE_LIST_HEAD(&EfiFreeDescriptorList);

    //
    // Get the blank platform memory map.
    //

    Status = EfiPlatformGetInitialMemoryMap(&PlatformMap, &PlatformMapSize);
    if (EFI_ERROR(Status)) {
        goto CoreInitializeMemoryServicesEnd;
    }

    //
    // Find the biggest free descriptor and add that one first.
    //

    FreeEntry = NULL;
    for (Index = 0; Index < PlatformMapSize; Index += 1) {
        Entry = &(PlatformMap[Index]);
        if ((Entry->Type == EfiConventionalMemory) &&
            (Entry->PhysicalStart < MAX_ADDRESS) &&
            ((FreeEntry == NULL) ||
             (Entry->NumberOfPages > FreeEntry->NumberOfPages))) {

            FreeEntry = Entry;
        }
    }

    if (FreeEntry == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto CoreInitializeMemoryServicesEnd;
    }

    EfipCoreAddMemoryDescriptor(FreeEntry->Type,
                                FreeEntry->PhysicalStart,
                                FreeEntry->NumberOfPages,
                                FreeEntry->Attribute);

    //
    // Now add all the other entries.
    //

    for (Index = 0; Index < PlatformMapSize; Index += 1) {
        Entry = &(PlatformMap[Index]);
        if (Entry == FreeEntry) {
            continue;
        }

        EfipCoreAddMemoryDescriptor(Entry->Type,
                                    Entry->PhysicalStart,
                                    Entry->NumberOfPages,
                                    Entry->Attribute);
    }

    Status = EfiCoreInitializePool();
    if (EFI_ERROR(Status)) {
        goto CoreInitializeMemoryServicesEnd;
    }

    //
    // Add the firmware image and stack as boot services code and data.
    //

    EntryAddress = (UINTN)FirmwareLowestAddress & ~EFI_PAGE_MASK;
    FirmwareSize += (UINTN)FirmwareLowestAddress & EFI_PAGE_MASK;
    Status = EfiCoreAllocatePages(AllocateAddress,
                                  EfiBootServicesCode,
                                  EFI_SIZE_TO_PAGES(FirmwareSize),
                                  &EntryAddress);

    if (EFI_ERROR(Status)) {
        RtlDebugPrint("Failed to add firmware image to memory map.\n");
        goto CoreInitializeMemoryServicesEnd;
    }

    EntryAddress = (UINTN)StackBase;

    ASSERT((EntryAddress & EFI_PAGE_MASK) == 0);
    ASSERT((StackSize & EFI_PAGE_MASK) == 0);

    Status = EfiCoreAllocatePages(AllocateAddress,
                                  EfiBootServicesData,
                                  EFI_SIZE_TO_PAGES(StackSize),
                                  &EntryAddress);

    if (EFI_ERROR(Status)) {
        RtlDebugPrint("Failed to add firmware stack to memory map.\n");
        goto CoreInitializeMemoryServicesEnd;
    }

CoreInitializeMemoryServicesEnd:
    return Status;
}

EFI_STATUS
EfiCoreTerminateMemoryServices (
    UINTN MapKey
    )

/*++

Routine Description:

    This routine terminates memory services.

Arguments:

    MapKey - Supplies the map key reported by the boot application. This is
        checked against the current map key to ensure the boot application has
        an up to date view of the world.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the map key is not valid or the memory map is
    not consistent.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_MEMORY_MAP_ENTRY Entry;
    EFI_STATUS Status;

    Status = EFI_SUCCESS;
    EfiCoreAcquireLock(&EfiMemoryLock);
    if (MapKey == EfiMemoryMapKey) {
        CurrentEntry = EfiMemoryMap.Next;
        while (CurrentEntry != &EfiMemoryMap) {
            Entry = LIST_VALUE(CurrentEntry, EFI_MEMORY_MAP_ENTRY, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            if ((Entry->Descriptor.Attribute & EFI_MEMORY_RUNTIME) != 0) {
                if ((Entry->Descriptor.Type == EfiACPIReclaimMemory) ||
                    (Entry->Descriptor.Type == EfiACPIMemoryNVS)) {

                    RtlDebugPrint("ExitBootServices: ACPI memory entry has "
                                  "Runtime attribute set!\n");

                    Status = EFI_INVALID_PARAMETER;
                    goto CoreTerminateMemoryServicesEnd;
                }

                if ((Entry->Descriptor.PhysicalStart &
                     (EFI_ACPI_RUNTIME_PAGE_ALLOCATION_ALIGNMENT - 1)) != 0) {

                    RtlDebugPrint("ExitBootServices: Runtime entry is not "
                                  "aligned.\n");

                    Status = EFI_INVALID_PARAMETER;
                    goto CoreTerminateMemoryServicesEnd;
                }
            }
        }

    //
    // The boot application has a stale copy of the memory map. Fail.
    //

    } else {
        Status = EFI_INVALID_PARAMETER;
    }

CoreTerminateMemoryServicesEnd:
    EfiCoreReleaseLock(&EfiMemoryLock);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipCoreAddMemoryDescriptor (
    EFI_MEMORY_TYPE Type,
    EFI_PHYSICAL_ADDRESS Start,
    UINT64 PageCount,
    UINT64 Attribute
    )

/*++

Routine Description:

    This routine is called to initialize the memory map and add descriptors.
    The first descriptor added must be general usable memory.

Arguments:

    Type - Supplies the type of memory to add.

    Start - Supplies the starting physical address. This must be page aligned.

    PageCount - Supplies the number of pages being described.

    Attribute - Supplies the memory attributes of the region.

Return Value:

    None.

--*/

{

    EFI_PHYSICAL_ADDRESS End;
    UINTN FreeIndex;
    UINTN Index;
    EFI_STATUS Status;

    if ((Start & EFI_PAGE_MASK) != 0) {
        return;
    }

    if ((Type >= EfiMaxMemoryType) && (Type < 0x7FFFFFFF)) {
        return;
    }

    EfiCoreAcquireLock(&EfiMemoryLock);
    End = Start + EFI_PAGES_TO_SIZE(PageCount) - 1;
    EfipCoreAddRange(Type, Start, End, Attribute);
    EfipCoreFlushMemoryMapStack();
    EfiCoreReleaseLock(&EfiMemoryLock);

    //
    // The rest of this function initializes the memory statistics.
    //

    if (EfiMemoryTypeInformationInitialized != FALSE) {
        return;
    }

    //
    // Loop through each memory type in the order specified by the type
    // information array.
    //

    for (Index = 0;
         EfiMemoryTypeInformation[Index].Type != EfiMaxMemoryType;
         Index += 1) {

        Type = (EFI_MEMORY_TYPE)(EfiMemoryTypeInformation[Index].Type);
        if ((UINT32)Type > EfiMaxMemoryType) {
            continue;
        }

        if (EfiMemoryTypeInformation[Index].PageCount == 0) {
            continue;
        }

        //
        // Allocate pages for the memory type from the top of available memory.
        //

        Status = EfiCoreAllocatePages(AllocateAnyPages,
                                      Type,
                                      EfiMemoryTypeInformation[Index].PageCount,
                                      &(EfiMemoryStatistics[Type].BaseAddress));

        if (EFI_ERROR(Status)) {

            //
            // If an error occurred, free all pages allocated for the previous
            // memory types.
            //

            for (FreeIndex = 0; FreeIndex < Index; FreeIndex += 1) {
                Type =
                    (EFI_MEMORY_TYPE)(EfiMemoryTypeInformation[FreeIndex].Type);

                if ((UINT32)Type > EfiMaxMemoryType) {
                    continue;
                }

                if (EfiMemoryTypeInformation[FreeIndex].PageCount != 0) {
                    EfiCoreFreePages(
                                EfiMemoryStatistics[Type].BaseAddress,
                                EfiMemoryTypeInformation[FreeIndex].PageCount);

                    EfiMemoryStatistics[Type].BaseAddress = 0;
                    EfiMemoryStatistics[Type].MaximumAddress = MAX_ADDRESS;
                }
            }

            return;
        }

        //
        // Compute the address at the top of the current statistics.
        //

        EfiMemoryStatistics[Type].MaximumAddress =
              EfiMemoryStatistics[Type].BaseAddress +
              EFI_PAGES_TO_SIZE(EfiMemoryTypeInformation[Index].PageCount) - 1;

        //
        // If the current base address is the lowest so far, update the default
        // max address.
        //

        if (EfiMemoryStatistics[Type].BaseAddress < EfiDefaultMaximumAddress) {
            EfiDefaultMaximumAddress =
                                     EfiMemoryStatistics[Type].BaseAddress - 1;
        }
    }

    //
    // There was enough system memory for all the memory types. Free those
    // allocated pages now, and now future allocations of that type will fit
    // into those slots. This avoids fragmentation.
    //

    for (Index = 0;
         EfiMemoryTypeInformation[Index].Type != EfiMaxMemoryType;
         Index += 1) {

        Type = (EFI_MEMORY_TYPE)(EfiMemoryTypeInformation[Index].Type);
        if ((UINT32)Type > EfiMaxMemoryType) {
            continue;
        }

        if (EfiMemoryTypeInformation[Index].PageCount == 0) {
            continue;
        }

        EfiCoreFreePages(EfiMemoryStatistics[Type].BaseAddress,
                         EfiMemoryTypeInformation[Index].PageCount);

        EfiMemoryStatistics[Type].PageCount =
                                     EfiMemoryTypeInformation[Index].PageCount;

        EfiMemoryTypeInformation[Index].PageCount = 0;
    }

    //
    // If the number of pages reserved for a memory type is zero, then all
    // allocations for that type should be in the default range.
    //

    for (Type = (EFI_MEMORY_TYPE)0;
         Type < EfiMaxMemoryType;
         Type += 1) {

        for (Index = 0;
             EfiMemoryTypeInformation[Index].Type != EfiMaxMemoryType;
             Index += 1) {

            if (Type == (EFI_MEMORY_TYPE)EfiMemoryTypeInformation[Index].Type) {
                EfiMemoryStatistics[Type].InformationIndex = Index;
            }
        }

        EfiMemoryStatistics[Type].CurrentPageCount = 0;
        if (EfiMemoryStatistics[Type].MaximumAddress == MAX_ADDRESS) {
            EfiMemoryStatistics[Type].MaximumAddress = EfiDefaultMaximumAddress;
        }
    }

    EfiMemoryTypeInformationInitialized = TRUE;
    return;
}

UINT64
EfipCoreFindFreePages (
    UINT64 MaxAddress,
    UINT64 PageCount,
    EFI_MEMORY_TYPE NewType,
    UINTN Alignment
    )

/*++

Routine Description:

    This routine attempts to find a consecutive range of free pages below the
    given maximum address.

Arguments:

    MaxAddress - Supplies the maximum address that the allocation must stay
        below.

    PageCount - Supplies the number of pages to allocate.

    NewType - Supplies the type of memory this range is going to be turned into.

    Alignment - Supplies the required alignment of the allocation.

Return Value:

    Returns the physical address of the base of the allocation on success.

    0 if the range was not found.

--*/

{

    UINT64 Start;

    //
    // First try to find free pages in the range where there are already
    // descriptors of this type hanging around.
    //

    if (((UINT32)NewType < EfiMaxMemoryType) &&
        (MaxAddress >= EfiMemoryStatistics[NewType].MaximumAddress)) {

        Start = EfipCoreFindFreePagesInRange(
                                    EfiMemoryStatistics[NewType].MaximumAddress,
                                    EfiMemoryStatistics[NewType].BaseAddress,
                                    PageCount,
                                    NewType,
                                    Alignment);

        if (Start != 0) {
            return Start;
        }
    }

    //
    // Attempt to find free pages in the default area.
    //

    if (MaxAddress >= EfiDefaultMaximumAddress) {
        Start = EfipCoreFindFreePagesInRange(EfiDefaultMaximumAddress,
                                             0,
                                             PageCount,
                                             NewType,
                                             Alignment);

        if (Start != 0) {
            if (Start < EfiDefaultBaseAddress) {
                EfiDefaultBaseAddress = Start;
            }

            return Start;
        }
    }

    //
    // Find free pages anywhere in the specified range. This is the most
    // permissive search. If this doesn't work, it's not happening.
    //

    Start = EfipCoreFindFreePagesInRange(MaxAddress,
                                         0,
                                         PageCount,
                                         NewType,
                                         Alignment);

    return Start;
}

UINT64
EfipCoreFindFreePagesInRange (
    UINT64 MaxAddress,
    UINT64 MinAddress,
    UINT64 PageCount,
    EFI_MEMORY_TYPE NewType,
    UINTN Alignment
    )

/*++

Routine Description:

    This routine attempts to find a consecutive range of free pages within the
    specified range.

Arguments:

    MaxAddress - Supplies the maximum address that the allocation must stay
        below.

    MinAddress - Supplies the minimum address that the allocation must stay
        at or above.

    PageCount - Supplies the number of pages to allocate.

    NewType - Supplies the type of memory this range is going to be turned into.

    Alignment - Supplies the required alignment of the allocation.

Return Value:

    Returns the physical address of the base of the allocation on success.

    0 if the range was not found.

--*/

{

    UINT64 ByteCount;
    PLIST_ENTRY CurrentEntry;
    PEFI_MEMORY_MAP_ENTRY Entry;
    UINT64 EntryEnd;
    UINT64 EntrySize;
    UINT64 EntryStart;
    UINT64 Target;

    if ((MaxAddress < EFI_PAGE_MASK) || (PageCount == 0)) {
        return 0;
    }

    //
    // Chop the max address down if it's not one below a page boundary.
    //

    if ((MaxAddress & EFI_PAGE_MASK) != EFI_PAGE_MASK) {
        MaxAddress -= EFI_PAGE_MASK + 1;
        MaxAddress &= ~EFI_PAGE_MASK;
        MaxAddress |= EFI_PAGE_MASK;
    }

    ByteCount = PageCount << EFI_PAGE_SHIFT;
    Target = 0;
    CurrentEntry = EfiMemoryMap.Next;
    while (CurrentEntry != &EfiMemoryMap) {
        Entry = LIST_VALUE(CurrentEntry, EFI_MEMORY_MAP_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Entry->Descriptor.Type != EfiConventionalMemory) {
            continue;
        }

        EntryStart = Entry->Descriptor.PhysicalStart;
        EntryEnd = EntryStart +
                   (Entry->Descriptor.NumberOfPages << EFI_PAGE_SHIFT);

        //
        // Skip descriptors that are outside of the requested range.
        //

        if ((EntryStart >= MaxAddress) || (EntryEnd < MinAddress)) {
            continue;
        }

        //
        // If the descriptor ends past the maximum allowed address, clip it.
        //

        if (EntryEnd > MaxAddress) {
            EntryEnd = MaxAddress;
        }

        EntryEnd = ((EntryEnd + 1) & (~(Alignment - 1))) - 1;

        //
        // If the entry is big enough, and does not dip below the minimum
        // address, then it works.
        //

        EntrySize = EntryEnd - EntryStart + 1;
        if (EntrySize >= ByteCount) {
            if ((EntryEnd - ByteCount + 1) < MinAddress) {
                continue;
            }

            //
            // If this is the highest match, save it.
            //

            if (EntryEnd > Target) {
                Target = EntryEnd;
            }
        }
    }

    if (Target == 0) {
        return 0;
    }

    ASSERT(Target > ByteCount);

    Target -= ByteCount - 1;
    if ((Target & EFI_PAGE_MASK) != 0) {
        return 0;
    }

    return Target;
}

EFI_STATUS
EfipCoreConvertPages (
    UINT64 Start,
    UINT64 PageCount,
    EFI_MEMORY_TYPE NewType
    )

/*++

Routine Description:

    This routine converts a given range to the specified type. The range must
    already exist in the memory map.

Arguments:

    Start - Supplies the first address in the range. This must be page aligned.

    PageCount - Supplies the number of pages in the range.

    NewType - Supplies the type to convert the pages to.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the page count is zero, the address is not
    aligned, or the type is invalid.

    EFI_NOT_FOUND if no existing descriptor covers the given region.

--*/

{

    UINT64 Attribute;
    UINT64 ByteCount;
    PLIST_ENTRY CurrentEntry;
    UINT64 End;
    PEFI_MEMORY_MAP_ENTRY Entry;
    UINT64 EntryEnd;
    UINT64 EntryStart;
    EFI_MEMORY_TYPE EntryType;
    UINTN InformationIndex;
    PEFI_MEMORY_MAP_ENTRY NewEntry;
    UINT64 RangeEnd;

    ByteCount = PageCount << EFI_PAGE_SHIFT;
    Entry = NULL;
    End = Start + ByteCount - 1;

    ASSERT((PageCount != 0) && ((Start & EFI_PAGE_MASK) == 0) &&
           (End > Start) && (EfiCoreIsLockHeld(&EfiMemoryLock) != FALSE));

    if ((PageCount == 0) || ((Start & EFI_PAGE_MASK) != 0) ||
        (Start > Start + ByteCount)) {

        return EFI_INVALID_PARAMETER;
    }

    //
    // Loop until the entire range is converted.
    //

    while (Start < End) {

        //
        // Loop through looking for the descriptor that contains this range.
        //

        CurrentEntry = EfiMemoryMap.Next;
        while (CurrentEntry != &EfiMemoryMap) {
            Entry = LIST_VALUE(CurrentEntry, EFI_MEMORY_MAP_ENTRY, ListEntry);
            EntryStart = Entry->Descriptor.PhysicalStart;
            EntryEnd = EntryStart +
                       (Entry->Descriptor.NumberOfPages << EFI_PAGE_SHIFT) - 1;

            if ((EntryStart <= Start) && (EntryEnd > Start)) {
                break;
            }

            CurrentEntry = CurrentEntry->Next;
        }

        if (CurrentEntry == &EfiMemoryMap) {
            return EFI_NOT_FOUND;
        }

        //
        // Convert the range to the end, or to the end of the descriptor if the
        // range covers more than the descriptor.
        //

        RangeEnd = End;
        if (EntryEnd < End) {
            RangeEnd = EntryEnd;
        }

        //
        // Verify the conversion is allowed.
        //

        EntryType = Entry->Descriptor.Type;
        if (NewType == EfiConventionalMemory) {
            if (EntryType == EfiConventionalMemory) {
                return EFI_NOT_FOUND;
            }

        } else {
            if (EntryType != EfiConventionalMemory) {
                return EFI_NOT_FOUND;
            }
        }

        //
        // Update the counters for the number of pages allocated to each
        // memory type.
        //

        if ((UINT32)EntryType < EfiMaxMemoryType) {
            if (((Start >= EfiMemoryStatistics[EntryType].BaseAddress) &&
                 (Start <= EfiMemoryStatistics[EntryType].MaximumAddress)) ||
                ((Start >= EfiDefaultBaseAddress) &&
                 (Start <= EfiDefaultMaximumAddress))) {

                if (PageCount >=
                    EfiMemoryStatistics[EntryType].CurrentPageCount) {

                    EfiMemoryStatistics[EntryType].CurrentPageCount = 0;

                } else {
                    EfiMemoryStatistics[EntryType].CurrentPageCount -=
                                                                     PageCount;
                }
            }
        }

        if ((UINT32)NewType < EfiMaxMemoryType) {
            if (((Start > EfiMemoryStatistics[NewType].BaseAddress) &&
                 (Start <= EfiMemoryStatistics[NewType].MaximumAddress)) ||
                ((Start >= EfiDefaultBaseAddress) &&
                 (Start <= EfiDefaultMaximumAddress))) {

                EfiMemoryStatistics[NewType].CurrentPageCount += PageCount;
                InformationIndex =
                                 EfiMemoryStatistics[NewType].InformationIndex;

                if (EfiMemoryStatistics[NewType].CurrentPageCount >
                    EfiMemoryTypeInformation[InformationIndex].PageCount) {

                    EfiMemoryTypeInformation[InformationIndex].PageCount =
                         (UINT32)EfiMemoryStatistics[NewType].CurrentPageCount;
                }
            }
        }

        //
        // Pull the requested range out of the descriptor.
        //

        if (EntryStart == Start) {
            EntryStart = RangeEnd + 1;

        } else if (EntryEnd == RangeEnd) {
            EntryEnd = Start - 1;

        //
        // The descriptor is being split in two. Clip the end of current one
        // and add a new one for the remainder.
        //

        } else {

            ASSERT(EfiDescriptorStackSize < EFI_DESCRIPTOR_STACK_SIZE);

            NewEntry = &(EfiDescriptorStack[EfiDescriptorStackSize]);
            EfiDescriptorStackSize += 1;
            NewEntry->Temporary = TRUE;
            NewEntry->Descriptor.Type = EntryType;
            NewEntry->Descriptor.PhysicalStart = RangeEnd + 1;
            NewEntry->Descriptor.VirtualStart = 0;
            NewEntry->Descriptor.NumberOfPages =
                                   (EntryEnd + 1 - RangeEnd) >> EFI_PAGE_SHIFT;

            NewEntry->Descriptor.Attribute = Entry->Descriptor.Attribute;
            EntryEnd = Start - 1;

            ASSERT(EntryStart < EntryEnd);

            INSERT_BEFORE(&(NewEntry->ListEntry), &EfiMemoryMap);
        }

        Attribute = Entry->Descriptor.Attribute;
        if (EntryStart == EntryEnd + 1) {
            EfipCoreRemoveMemoryMapEntry(Entry);
            Entry = NULL;

        } else {
            Entry->Descriptor.PhysicalStart = EntryStart;
            Entry->Descriptor.NumberOfPages =
                                 (EntryEnd + 1 - EntryStart) >> EFI_PAGE_SHIFT;
        }

        //
        // Add the new range in.
        //

        EfipCoreAddRange(NewType, Start, RangeEnd, Attribute);

        //
        // Flush the temporary descriptors out to real descriptors.
        //

        EfipCoreFlushMemoryMapStack();

        //
        // Move on to the next range.
        //

        Start = RangeEnd + 1;
    }

    return EFI_SUCCESS;
}

VOID
EfipCoreAddRange (
    EFI_MEMORY_TYPE Type,
    EFI_PHYSICAL_ADDRESS Start,
    EFI_PHYSICAL_ADDRESS End,
    UINT64 Attribute
    )

/*++

Routine Description:

    This routine adds a range to the memory map. The range must not already
    exist in the memory map.

Arguments:

    Type - Supplies the memory type of the range.

    Start - Supplies the starting address of the range. This must be page
        aligned.

    End - Supplies the ending address of the range, inclusive. This must be the
        last byte of a page.

    Attribute - Supplies the attributes of the range to add.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_MEMORY_MAP_ENTRY Entry;
    UINT64 EntryEnd;
    UINT64 EntryStart;
    PEFI_MEMORY_MAP_ENTRY NewEntry;

    ASSERT((Start & EFI_PAGE_MASK) == 0);
    ASSERT(End > Start);
    ASSERT(EfiCoreIsLockHeld(&EfiMemoryLock) != FALSE);

    //
    // If free memory is being added that includes page zero, zero out that
    // page.
    //

    if ((Type == EfiConventionalMemory) &&
        (Start == 0) &&
        (End >= EFI_PAGE_SIZE - 1)) {

        EfiCoreSetMemory((VOID *)(UINTN)Start, EFI_PAGE_SIZE, 0);
    }

    //
    // The memory map is being altered, so update the map key.
    //

    EfiMemoryMapKey += 1;

    //
    // Notify the event group wired to listen for memory map changes.
    // Since the TPL is raised the notification functions will only be called
    // after the lock is released.
    //

    EfipCoreNotifySignalList(&EfiEventMemoryMapChangeGuid);

    //
    // Look for descriptors to coalesce with.
    //

    CurrentEntry = EfiMemoryMap.Next;
    while (CurrentEntry != &EfiMemoryMap) {
        Entry = LIST_VALUE(CurrentEntry, EFI_MEMORY_MAP_ENTRY, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Entry->Descriptor.Type != Type) {
            continue;
        }

        if (Entry->Descriptor.Attribute != Attribute) {
            continue;
        }

        EntryStart = Entry->Descriptor.PhysicalStart;
        EntryEnd = EntryStart +
                   (Entry->Descriptor.NumberOfPages << EFI_PAGE_SHIFT) - 1;

        if (EntryEnd + 1 == EntryStart) {
            Start = EntryStart;
            EfipCoreRemoveMemoryMapEntry(Entry);

        } else if (EntryStart == End + 1) {
            End = EntryEnd;
            EfipCoreRemoveMemoryMapEntry(Entry);
        }
    }

    //
    // Add the new descriptor.
    //

    ASSERT(EfiDescriptorStackSize < EFI_DESCRIPTOR_STACK_SIZE);

    NewEntry = &(EfiDescriptorStack[EfiDescriptorStackSize]);
    EfiDescriptorStackSize += 1;
    NewEntry->Temporary = TRUE;
    NewEntry->Descriptor.Type = Type;
    NewEntry->Descriptor.PhysicalStart = Start;
    NewEntry->Descriptor.VirtualStart = 0;
    NewEntry->Descriptor.NumberOfPages = (End + 1 - Start) >> EFI_PAGE_SHIFT;
    NewEntry->Descriptor.Attribute = Attribute;
    INSERT_BEFORE(&(NewEntry->ListEntry), &EfiMemoryMap);
    return;
}

EFI_MEMORY_DESCRIPTOR *
EfipCoreMergeMemoryMapDescriptor (
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    EFI_MEMORY_DESCRIPTOR *LastDescriptor,
    UINTN DescriptorSize
    )

/*++

Routine Description:

    This routine checks to see if memory descriptros can be merged together.
    Descriptors qualify for merging if they are adjacent and have the same
    attributes.

Arguments:

    MemoryMap - Supplies a pointer to the start of the memory map.

    LastDescriptor - Supplies a pointer to the last descriptor in the map.

    DescriptorSize - Supplies the size of an individual EFI memory descriptor.

Return Value:

    Returns a pointer to the next available descriptor in the memory map.

--*/

{

    //
    // Loop over each entry in the map.
    //

    while (MemoryMap != LastDescriptor) {
        if ((MemoryMap->Type != LastDescriptor->Type) ||
            (MemoryMap->Attribute != LastDescriptor->Attribute)) {

            MemoryMap =
                (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + DescriptorSize);

            continue;
        }

        //
        // Check to see if the given descriptor is immediately above this
        // descriptor.
        //

        if (MemoryMap->PhysicalStart +
            EFI_PAGES_TO_SIZE((UINTN)MemoryMap->NumberOfPages) ==
            LastDescriptor->PhysicalStart) {

            MemoryMap->NumberOfPages += LastDescriptor->NumberOfPages;
            return LastDescriptor;
        }

        //
        // Check to see if the last descriptor is immediately below this one.
        //

        if (MemoryMap->PhysicalStart -
            EFI_PAGES_TO_SIZE((UINTN)LastDescriptor->NumberOfPages) ==
            LastDescriptor->PhysicalStart) {

            MemoryMap->PhysicalStart = LastDescriptor->PhysicalStart;
            MemoryMap->VirtualStart = LastDescriptor->VirtualStart;
            MemoryMap->NumberOfPages += LastDescriptor->NumberOfPages;
            return LastDescriptor;
        }

        //
        // Move on to the next descriptor.
        //

        MemoryMap =
                (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + DescriptorSize);
    }

    //
    // Nothing coalesces, the next descriptor is the one after the last one.
    //

    LastDescriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)LastDescriptor +
                                               DescriptorSize);

    return LastDescriptor;
}

VOID
EfipCoreRemoveMemoryMapEntry (
    PEFI_MEMORY_MAP_ENTRY Entry
    )

/*++

Routine Description:

    This routine removes a descriptor entry and places it on a free list for
    later use.

Arguments:

    Entry - Supplies a pointer to the entry to remove.

Return Value:

    None.

--*/

{

    LIST_REMOVE(&(Entry->ListEntry));
    Entry->ListEntry.Next = NULL;
    if (Entry->Temporary == FALSE) {
        INSERT_BEFORE(&(Entry->ListEntry), &EfiFreeDescriptorList);
    }

    return;
}

VOID
EfipCoreFlushMemoryMapStack (
    VOID
    )

/*++

Routine Description:

    This routine replaces all temporary memory map entries with real allocated
    memory map entries.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PEFI_MEMORY_MAP_ENTRY Entry;
    PEFI_MEMORY_MAP_ENTRY NewEntry;
    PEFI_MEMORY_MAP_ENTRY StackEntry;

    //
    // Avoid re-entering this function.
    //

    if (EfiDescriptorStackFreeInProgress != FALSE) {
        return;
    }

    EfiDescriptorStackFreeInProgress = TRUE;
    while (EfiDescriptorStackSize != 0) {
        NewEntry = EfipCoreAllocateMemoryMapEntry();

        ASSERT(NewEntry != NULL);

        EfiDescriptorStackSize -= 1;
        StackEntry = &(EfiDescriptorStack[EfiDescriptorStackSize]);

        //
        // If it's in the memory map, then create a replacement copy.
        //

        if (StackEntry->ListEntry.Next != NULL) {
            LIST_REMOVE(&(StackEntry->ListEntry));
            StackEntry->ListEntry.Next = NULL;
            EfiCoreCopyMemory(NewEntry,
                              StackEntry,
                              sizeof(EFI_MEMORY_MAP_ENTRY));

            NewEntry->Temporary = FALSE;

            //
            // Find the proper insertion location.
            //

            CurrentEntry = EfiMemoryMap.Next;
            while (CurrentEntry != &EfiMemoryMap) {
                Entry = LIST_VALUE(CurrentEntry,
                                   EFI_MEMORY_MAP_ENTRY,
                                   ListEntry);

                if ((Entry->Temporary == FALSE) &&
                    (Entry->Descriptor.PhysicalStart >
                     NewEntry->Descriptor.PhysicalStart)) {

                    break;
                }

                CurrentEntry = CurrentEntry->Next;
            }

            INSERT_BEFORE(&(NewEntry->ListEntry), CurrentEntry);

        //
        // This descriptor was already removed, so the descriptor just
        // allocated isn't needed.
        //

        } else {
            INSERT_AFTER(&(NewEntry->ListEntry), &EfiFreeDescriptorList);
        }
    }

    EfiDescriptorStackFreeInProgress = FALSE;
    return;
}

PEFI_MEMORY_MAP_ENTRY
EfipCoreAllocateMemoryMapEntry (
    VOID
    )

/*++

Routine Description:

    This routine allocates a new memory map entry. It uses the free list to
    reuse previous descriptors. If that's empty, it allocates a page from the
    memory map and uses that to create more descriptors.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PEFI_MEMORY_MAP_ENTRY Entries;
    PEFI_MEMORY_MAP_ENTRY Entry;
    UINTN EntryCount;
    UINTN Index;

    if (LIST_EMPTY(&EfiFreeDescriptorList) != FALSE) {
        Entries = EfiCoreAllocatePoolPages(
                                  EfiBootServicesData,
                                  EFI_SIZE_TO_PAGES(EFI_MEMORY_EXPANSION_SIZE),
                                  EFI_MEMORY_EXPANSION_SIZE);

        if (Entries != NULL) {
            EntryCount = EFI_MEMORY_EXPANSION_SIZE /
                         sizeof(EFI_MEMORY_MAP_ENTRY);

            for (Index = 0; Index < EntryCount; Index += 1) {
                INSERT_BEFORE(&(Entries[Index].ListEntry),
                              &EfiFreeDescriptorList);
            }

        } else {

            //
            // The system just exhausted all memory, and won't do well after
            // this.
            //

            ASSERT(FALSE);

            return NULL;
        }
    }

    ASSERT(LIST_EMPTY(&EfiFreeDescriptorList) == FALSE);

    Entry = LIST_VALUE(EfiFreeDescriptorList.Next,
                       EFI_MEMORY_MAP_ENTRY,
                       ListEntry);

    LIST_REMOVE(&(Entry->ListEntry));
    return Entry;
}

VOID
EfipDebugPrintMemoryMap (
    EFI_MEMORY_DESCRIPTOR *Map,
    UINTN MapSize,
    UINTN DescriptorSize
    )

/*++

Routine Description:

    This routine prints an EFI memory map out to the debugger.

Arguments:

    Map - Supplies a pointer to the memory map.

    MapSize - Supplies the size of the map in bytes.

    DescriptorSize - Supplies the size of a single descriptor.

Return Value:

    None.

--*/

{

    EFI_MEMORY_DESCRIPTOR *Descriptor;
    UINT64 Megabytes;
    UINTN Offset;
    UINT64 TotalPages;

    RtlDebugPrint("EFI Memory map at 0x%08I64x\n", Map);
    TotalPages = 0;
    Offset = 0;
    while (Offset < MapSize) {
        Descriptor = (EFI_MEMORY_DESCRIPTOR *)((VOID *)Map + Offset);
        EfipDebugPrintMemoryDescriptor(Descriptor);
        TotalPages += Descriptor->NumberOfPages;
        Offset += DescriptorSize;
    }

    Megabytes = (TotalPages << EFI_PAGE_SHIFT) / (1024ULL * 1024ULL);
    RtlDebugPrint("Total Pages: 0x%I64x (%I64dMB)\n\n", TotalPages, Megabytes);
    return;
}

VOID
EfipDebugPrintMemoryDescriptor (
    EFI_MEMORY_DESCRIPTOR *Descriptor
    )

/*++

Routine Description:

    This routine prints an EFI memory descriptor out to the debugger.

Arguments:

    Descriptor - Supplies a pointer to the descriptor to print.

Return Value:

    None.

--*/

{

    CHAR8 *TypeString;

    switch (Descriptor->Type) {
    case EfiReservedMemoryType:
        TypeString = "ReservedMemoryType";
        break;

    case EfiLoaderCode:
        TypeString = "LoaderCode";
        break;

    case EfiLoaderData:
        TypeString = "LoaderData";
        break;

    case EfiBootServicesCode:
        TypeString = "BootServicesCode";
        break;

    case EfiBootServicesData:
        TypeString = "BootServicesData";
        break;

    case EfiRuntimeServicesCode:
        TypeString = "RuntimeServicesCode";
        break;

    case EfiRuntimeServicesData:
        TypeString = "RuntimeServicesData";
        break;

    case EfiConventionalMemory:
        TypeString = "ConventionalMemory";
        break;

    case EfiUnusableMemory:
        TypeString = "UnusableMemory";
        break;

    case EfiACPIReclaimMemory:
        TypeString = "ACPIReclaimMemory";
        break;

    case EfiACPIMemoryNVS:
        TypeString = "ACPIMemoryNVS";
        break;

    case EfiMemoryMappedIO:
        TypeString = "MemoryMappedIO";
        break;

    case EfiMemoryMappedIOPortSpace:
        TypeString = "MemoryMappedIOPortSpace";
        break;

    case EfiPalCode:
        TypeString = "PalCode";
        break;

    default:
        TypeString = "INVALID";
        break;
    }

    RtlDebugPrint("%24s PA 0x%8I64x (VA 0x%I64x) PageCount 0x%8I64x "
                  "Attr 0x%x\n",
                  TypeString,
                  Descriptor->PhysicalStart,
                  Descriptor->VirtualStart,
                  Descriptor->NumberOfPages,
                  Descriptor->Attribute);

    return;
}


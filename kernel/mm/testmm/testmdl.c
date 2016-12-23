/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testmdl.c

Abstract:

    This module contains tests for the memory descriptor manipulation functions.

Author:

    Evan Green 27-Jul-2012

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "../mmp.h"
#include "testmm.h"

#include <stdio.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MDL_PRINT printf

#define TEST_MDL_DESCRIPTOR_COUNT 100
#define MDL_TEST_ALLOCATION_COUNT 50000

//
// ----------------------------------------------- Internal Function Prototypes
//

UINTN
MmpMdGetFreeBinIndex (
    ULONGLONG Size
    );

PCSTR
PrintMemoryType (
    MEMORY_TYPE MemoryType
    );

VOID
ValidateMdlIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the iteration context when validating an MDL.

Members:

    DescriptorCount - Stores the computed number of descriptors.

    Free - Stores the computed free space.

    PreviousEnd - Stores the previous descriptor's ending address.

    PreviousType - Stores the previous descriptor's type.

    Total - Stores the total space the descriptor describes.

    Valid - Stores a boolean indicating if validation has failed.

--*/

typedef struct _MDL_VALIDATION_CONTEXT {
    ULONG DescriptorCount;
    ULONGLONG Free;
    ULONGLONG PreviousEnd;
    MEMORY_TYPE PreviousType;
    ULONGLONG Total;
    BOOL Valid;
} MDL_VALIDATION_CONTEXT, *PMDL_VALIDATION_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

MEMORY_DESCRIPTOR TestMdlDescriptors[TEST_MDL_DESCRIPTOR_COUNT];

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestMdls (
    VOID
    )

/*++

Routine Description:

    This routine tests memory descriptor lists.

Arguments:

    None.

Return Value:

    Returns the number of test failures.

--*/

{

    ULONGLONG Address;
    ULONG AllocationIndex;
    ULONGLONG BaseAddress;
    ULONG DescriptorIndex;
    MEMORY_DESCRIPTOR_LIST Mdl;
    MEMORY_DESCRIPTOR NewDescriptor;
    ULONG PageShift;
    ULONG PageSize;
    KSTATUS Status;
    ULONG TestsFailed;
    MEMORY_TYPE Type;

    PageShift = MmPageShift();
    PageSize = MmPageSize();
    TestsFailed = 0;
    MmMdInitDescriptorList(&Mdl, MdlAllocationSourceNone);
    MmMdAddFreeDescriptorsToMdl(&Mdl,
                                TestMdlDescriptors,
                                sizeof(TestMdlDescriptors));

    //
    // Just insert a bunch of MDLs that don't overlap.
    //

    for (DescriptorIndex = 0; DescriptorIndex < 10; DescriptorIndex += 1) {
        BaseAddress = 0x90000 - (DescriptorIndex * 0x10000);
        MmMdInitDescriptor(&NewDescriptor,
                           BaseAddress,
                           BaseAddress + 0x5000,
                           MemoryTypeFree);

        Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
        if (!KSUCCESS(Status)) {
            printf("Error adding standard descriptor to MDL: Status = %d.\n",
                   Status);

            TestsFailed += 1;
        }
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Add a descriptor that requires coalescing 3 descriptors and moving the
    // base address and size.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x6e << PageShift,
                       0x96 << PageShift,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to correctly add a descriptor that requires "
               "coalescing\n");

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Add a descriptor that's just touching two descriptors.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x5 << PageShift,
                       0x10 << PageShift,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to correctly add a descriptor that requires "
               "coalescing\n");

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Add a descriptor that's completely contained in another descriptor.
    // Use the last one.
    //

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to correctly add a descriptor that was "
               "completely contained in an existing descriptor.\n");

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Add a descriptor that's adjacent to two other descriptors of different
    // types.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x25 << PageShift,
                       0x30 << PageShift,
                       MemoryTypeFirmwarePermanent);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to correctly add a descriptor that requires "
               "coalescing\n");

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Add a descriptor somewhere in the middle.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x36 << PageShift,
                       0x37 << PageShift,
                       MemoryTypeAcpiTables);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to correctly add a descriptor that requires "
               "coalescing\n");

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Add a descriptor that coalesces but is completely contained within
    // existing descriptors.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x41 << PageShift,
                       0x95 << PageShift,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to add a descriptor that coalesces but is "
               "contained within existing descriptors.\n");

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Force add a basic descriptor.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0xa0 << PageShift,
                       0xa5 << PageShift,
                       MemoryTypeFree);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to force add a basic descriptor: Status = %d\n",
               Status);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Force add a descriptor that splits an existing descriptor.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x3 << PageShift,
                       0x5 << PageShift,
                       MemoryTypeBad);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to force add a descriptor that splits an "
               "existing descriptor: Status = %d\n",
               Status);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Force add a descriptor that splits an existing descriptor, but only on
    // one side.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x25 << PageShift,
                       0x28 << PageShift,
                       MemoryTypeBad);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to force add a descriptor that splits an "
               "existing descriptor: Status = %d\n",
               Status);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Force add a descriptor that spans several descriptors and requires
    // coalescing on both sides.
    //

    MmMdInitDescriptor(&NewDescriptor,
                       0x5 << PageShift,
                       0x25 << PageShift,
                       MemoryTypeBad);

    Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
    if (!KSUCCESS(Status)) {
        printf("Error: Failed to force add a descriptor that splits an "
               "existing descriptor: Status = %d\n",
               Status);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Attempt to allocate one page with no alignment requirement.
    //

    Status = MmMdAllocateFromMdl(&Mdl,
                                 &Address,
                                 PageSize,
                                 PageSize,
                                 0,
                                 MAX_UINTN,
                                 MemoryTypeHardware,
                                 AllocationStrategyAnyAddress);

    if (!KSUCCESS(Status)) {
        printf("Error: Failed to allocate a page with no alignment: "
               "Status = %d.\n",
               Status);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Allocate a page on a 2-page boundary, which requires skipping a free
    // descriptor.
    //

    Status = MmMdAllocateFromMdl(&Mdl,
                                 &Address,
                                 2 << PageShift,
                                 2 << PageShift,
                                 0,
                                 MAX_UINTN,
                                 MemoryTypeReserved,
                                 AllocationStrategyAnyAddress);

    if ((!KSUCCESS(Status)) || ((Address & 0x1FFF) != 0)) {
        printf("Error: Failed to allocate a page with no alignment: "
               "Status = %d. Address = 0x%llx\n",
               Status,
               Address);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Attempt to allocate 1 page on a 4-page alignment. This should work, but
    // requires splitting the first free descriptor.
    //

    Status = MmMdAllocateFromMdl(&Mdl,
                                 &Address,
                                 PageSize,
                                 4 << PageShift,
                                 0,
                                 MAX_UINTN,
                                 MemoryTypeHardware,
                                 AllocationStrategyAnyAddress);

    if ((!KSUCCESS(Status)) || ((Address & 0x3FFF) != 0)) {
        printf("Error: Failed to allocate 1 4-page aligned page. "
               "Status = %d, Address = 0x%llx.\n",
               Status,
               Address);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Now attempt to allocate 4 pages at a 10 page alignment.
    //

    Status = MmMdAllocateFromMdl(&Mdl,
                                 &Address,
                                 4 << PageShift,
                                 0x10 << PageShift,
                                 0,
                                 MAX_UINTN,
                                 MemoryTypeHardware,
                                 AllocationStrategyAnyAddress);

    if ((!KSUCCESS(Status)) || ((Address & 0xFFFF) != 0)) {
        printf("Error: Failed to allocate 4 10-page aligned pages. "
               "Status = %d, Address = 0x%llx.\n",
               Status,
               Address);

        TestsFailed += 1;
    }

    if (ValidateMdl(&Mdl) == FALSE) {
        TestsFailed += 1;
    }

    //
    // Make a bunch of random allocates and frees.
    //

    for (AllocationIndex = 0;
         AllocationIndex < MDL_TEST_ALLOCATION_COUNT;
         AllocationIndex += 1) {

        Address = rand();
        Type = MemoryTypeReserved;
        if ((Address & 0x1) != 0) {
            Type = MemoryTypeFree;
            Address &= ~0x1;
        }

        MmMdInitDescriptor(&NewDescriptor,
                           Address << 12,
                           (Address + rand() + 1) << 12,
                           Type);

        Status = MmMdAddDescriptorToList(&Mdl, &NewDescriptor);
        if (!KSUCCESS(Status)) {
            printf("Failed to add %llx %llx %d to MDL: %d\n",
                   NewDescriptor.BaseAddress,
                   NewDescriptor.Size,
                   Type,
                   Status);

            MmMdPrintMdl(&Mdl);
            TestsFailed += 1;
        }

        if (ValidateMdl(&Mdl) == FALSE) {
            MmMdPrintMdl(&Mdl);
            TestsFailed += 1;
            break;
        }
    }

    //
    // Tear down the MDL.
    //

    MmMdDestroyDescriptorList(&Mdl);
    return TestsFailed;
}

BOOL
ValidateMdl (
    PMEMORY_DESCRIPTOR_LIST Mdl
    )

/*++

Routine Description:

    This routine ensures that all entries of an MDL are valid and in order.

Arguments:

    Mdl - Supplies the memory descriptor list to validate.

Return Value:

    TRUE if the MDL is correct.

    FALSE if something was invalid about the MDL.

--*/

{

    PLIST_ENTRY Bin;
    UINTN BinIndex;
    MDL_VALIDATION_CONTEXT Context;
    PLIST_ENTRY CurrentEntry;
    PMEMORY_DESCRIPTOR Descriptor;
    BOOL Result;

    Result = RtlValidateRedBlackTree(&(Mdl->Tree));
    if (Result == FALSE) {
        printf("Error: MDL tree is invalid.\n");
    }

    RtlZeroMemory(&Context, sizeof(MDL_VALIDATION_CONTEXT));
    Context.Valid = TRUE;
    MmMdIterate(Mdl, ValidateMdlIterationRoutine, &Context);
    if (Context.Valid == FALSE) {
        Result = FALSE;
    }

    if (Context.DescriptorCount != Mdl->DescriptorCount) {
        printf("Error: Found %d descriptors, but %d were reported by the MDL."
               "\n",
               Context.DescriptorCount,
               Mdl->DescriptorCount);

        Result = FALSE;
    }

    if (Context.Total != Mdl->TotalSpace) {
        printf("Error: MDL reported %llx total space, but %llx total space "
               "calculated.\n",
               Mdl->TotalSpace,
               Context.Total);

        Result = FALSE;
    }

    if (Context.Free != Mdl->FreeSpace) {
        printf("Error: MDL reported %llx free space, but %llx free space "
               "calculated.\n",
               Mdl->FreeSpace,
               Context.Free);

        Result = FALSE;
    }

    //
    // Count up the free list entries and make sure they're not marked as used.
    //

    Context.DescriptorCount = 0;
    CurrentEntry = Mdl->UnusedListHead.Next;
    while (CurrentEntry != &(Mdl->UnusedListHead)) {
        Context.DescriptorCount += 1;
        Descriptor = LIST_VALUE(CurrentEntry, MEMORY_DESCRIPTOR, FreeListEntry);
        if ((Descriptor->Flags & DESCRIPTOR_FLAG_USED) != 0) {
            printf("Error: Found an active descriptor in an MDL free list.\n");
            Result = FALSE;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (Context.DescriptorCount != Mdl->UnusedDescriptorCount) {
        printf("Error: Found %d free descriptors, but %d were reported by the "
               "MDL.\n",
               Context.DescriptorCount,
               Mdl->UnusedDescriptorCount);

        Result = FALSE;
    }

    //
    // Also check the free bins.
    //

    for (BinIndex = 0; BinIndex < MDL_BIN_COUNT; BinIndex += 1) {
        Bin = &(Mdl->FreeLists[BinIndex]);
        CurrentEntry = Bin->Next;
        while (CurrentEntry != Bin) {
            Descriptor = LIST_VALUE(CurrentEntry,
                                    MEMORY_DESCRIPTOR,
                                    FreeListEntry);

            CurrentEntry = CurrentEntry->Next;
            if (MmpMdGetFreeBinIndex(Descriptor->Size) != BinIndex) {
                printf("Error: Descriptor %llx Size %llx belongs on bin "
                       "%ld, not bin %ld.\n",
                       Descriptor->BaseAddress,
                       Descriptor->Size,
                       MmpMdGetFreeBinIndex(Descriptor->Size),
                       BinIndex);

                Result = FALSE;
            }

            if ((Descriptor->Type != MemoryTypeFree) ||
                ((Descriptor->Flags & DESCRIPTOR_FLAG_USED) == 0)) {

                printf("Error: Type %d is not free, or flags %d is not used.\n",
                       Descriptor->Type,
                       Descriptor->Flags);

                Result = FALSE;
            }
        }
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

PCSTR
PrintMemoryType (
    MEMORY_TYPE MemoryType
    )

/*++

Routine Description:

    This routine returns a printable string associated with a memory type.

Arguments:

    MemoryType - Supplies the memory type.

Return Value:

    Returns a string describing the memory type.

--*/

{

    switch (MemoryType) {
    case MemoryTypeFree:
        return "Free Memory";

    case MemoryTypeReserved:
        return "Reserved";

    case MemoryTypeFirmwareTemporary:
        return "Firmware Temporary";

    case MemoryTypeFirmwarePermanent:
        return "Firmware Permanent";

    case MemoryTypeAcpiTables:
        return "ACPI Tables";

    case MemoryTypeAcpiNvStorage:
        return "ACPI Nonvolatile Storage";

    case MemoryTypeBad:
        return "Bad Memory";

    case MemoryTypeLoaderTemporary:
        return "Loader Temporary";

    case MemoryTypeLoaderPermanent:
        return "Loader Permanent";

    case MemoryTypePageTables:
        return "Page Tables";

    case MemoryTypeBootPageTables:
        return "Boot Page Tables";

    case MemoryTypeMmStructures:
        return "MM Init Structures";

    case MemoryTypeNonPagedPool:
        return "Non-paged Pool";

    case MemoryTypePagedPool:
        return "Paged Pool";

    case MemoryTypeHardware:
        return "Hardware";

    case MemoryTypeIoBuffer:
        return "IO Buffer";

    default:
        break;
    }

    return "Unknown Memory Type";
}

VOID
ValidateMdlIterationRoutine (
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

    PMDL_VALIDATION_CONTEXT ValidationContext;

    ValidationContext = Context;
    if ((Descriptor->Flags & DESCRIPTOR_FLAG_USED) == 0) {
        printf("Error: Found an inactive descriptor in an MDL.\n");
        ValidationContext->Valid = FALSE;
    }

    if (Descriptor->Size == 0) {
        printf("Error: found descriptor with base 0x%llx and size 0!\n",
               Descriptor->BaseAddress);

        ValidationContext->Valid = FALSE;
    }

    if (Descriptor->BaseAddress < ValidationContext->PreviousEnd) {
        printf("Descriptor out of order! Base: 0x%llx, Previous End: "
               "0x%llx\n",
               Descriptor->BaseAddress,
               ValidationContext->PreviousEnd);

        ValidationContext->Valid = FALSE;
    }

    if ((Descriptor->BaseAddress == ValidationContext->PreviousEnd) &&
        (Descriptor->Type == ValidationContext->PreviousType)) {

        printf("Error: found adjacent descriptors with the same type that "
               "should have been coalesced!\n");

        printf("    %13llx  %13llx  %8llx (PreviousEnd %llx)\n",
               Descriptor->BaseAddress,
               Descriptor->BaseAddress + Descriptor->Size,
               Descriptor->Size,
               ValidationContext->PreviousEnd);

        ValidationContext->Valid = FALSE;
    }

    ValidationContext->Total += Descriptor->Size;
    if (Descriptor->Type == MemoryTypeFree) {
        ValidationContext->Free += Descriptor->Size;
    }

    ValidationContext->PreviousEnd = Descriptor->BaseAddress + Descriptor->Size;
    ValidationContext->PreviousType = Descriptor->Type;
    ValidationContext->DescriptorCount += 1;
    return;
}


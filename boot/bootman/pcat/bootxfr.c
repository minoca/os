/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootxfr.c

Abstract:

    This module implements support for transition between the boot manager and
    another boot application.

Author:

    Evan Green 24-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "firmware.h"
#include "bootlib.h"
#include "bootman.h"
#include "bios.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EXTRA_BOOT_REGION_COUNT 4

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the iteration context for copying the memory
    descriptor list for the boot block.

Members:

    RegionCount - Stores the current region count.

    AllocatedRegionCount - Stores the number of allocated regions in the array.

    RegionArray - Stores the allocated array of regions.

--*/

typedef struct _BOOT_BLOCK_DESCRIPTOR_CONTEXT {
    UINTN RegionCount;
    UINTN AllocatedRegionCount;
    PBOOT_RESERVED_REGION RegionArray;
} BOOT_BLOCK_DESCRIPTOR_CONTEXT, *PBOOT_BLOCK_DESCRIPTOR_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BmpFwCreatePageTables (
    PBOOT_INITIALIZATION_BLOCK Parameters
    );

VOID
BmpFwBootBlockDescriptorIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BmpFwInitializeBootBlock (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_VOLUME OsVolume
    )

/*++

Routine Description:

    This routine initializes the boot initialization block that is passed when
    control is handed off to the next boot application.

Arguments:

    Parameters - Supplies a pointer to the boot initialization block.

    OsVolume - Supplies a pointer to the open volume containing the application
        to be launched.

Return Value:

    Status code.

--*/

{

    ULONG AllocatedRegionCount;
    UINTN AllocationSize;
    BOOT_BLOCK_DESCRIPTOR_CONTEXT Context;
    KSTATUS Status;

    RtlZeroMemory(&Context, sizeof(BOOT_BLOCK_DESCRIPTOR_CONTEXT));

    //
    // Loop through the memory map once to determine the number of reserved
    // regions.
    //

    MmMdIterate(&BoMemoryMap,
                BmpFwBootBlockDescriptorIterationRoutine,
                &Context);

    AllocatedRegionCount = Context.RegionCount + EXTRA_BOOT_REGION_COUNT;

    //
    // Allocate space for the descriptor array.
    //

    AllocationSize = AllocatedRegionCount * sizeof(BOOT_RESERVED_REGION);
    Context.RegionArray = BoAllocateMemory(AllocationSize);
    if (Context.RegionArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto FwInitializeBootBlockEnd;
    }

    RtlZeroMemory(Context.RegionArray, AllocationSize);
    Context.AllocatedRegionCount = AllocatedRegionCount;
    Context.RegionCount = 0;

    //
    // Allocate page tables if transferring to a 64-bit application.
    //

    if ((Parameters->Flags & BOOT_INITIALIZATION_FLAG_64BIT) != 0) {
        Status = BmpFwCreatePageTables(Parameters);
        if (!KSUCCESS(Status)) {
            goto FwInitializeBootBlockEnd;
        }
    }

    //
    // Loop through the descriptors again and mark all the regions used by this
    // and previous boot applications.
    //

    MmMdIterate(&BoMemoryMap,
                BmpFwBootBlockDescriptorIterationRoutine,
                &Context);

    Parameters->ReservedRegions = (UINTN)(Context.RegionArray);
    Parameters->ReservedRegionCount = Context.RegionCount;
    FwpPcatGetDiskInformation(OsVolume->DiskHandle,
                              &(Parameters->DriveNumber),
                              &(Parameters->PartitionOffset));

    Status = STATUS_SUCCESS;

FwInitializeBootBlockEnd:
    if (!KSUCCESS(Status)) {
        if (Context.RegionArray != NULL) {
            BoFreeMemory(Context.RegionArray);
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BmpFwBootBlockDescriptorIterationRoutine (
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

    PBOOT_BLOCK_DESCRIPTOR_CONTEXT BootContext;
    PBOOT_RESERVED_REGION Region;

    BootContext = Context;

    //
    // Skip all except interesting descriptors.
    //

    if ((Descriptor->Type == MemoryTypeFirmwareTemporary) ||
        (Descriptor->Type == MemoryTypeLoaderTemporary) ||
        (Descriptor->Type == MemoryTypeLoaderPermanent)) {

        //
        // If there's a region array, fill this in. Otherwise, just count.
        //

        if (BootContext->RegionArray != NULL) {

            ASSERT(BootContext->RegionCount <
                   BootContext->AllocatedRegionCount);

            Region = &(BootContext->RegionArray[BootContext->RegionCount]);
            Region->Address = Descriptor->BaseAddress;
            Region->Size = Descriptor->Size;
        }

        BootContext->RegionCount += 1;
    }

    return;
}


/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootmem.c

Abstract:

    This module implements general memory management support for the Boot
    Library.

Author:

    Evan Green 19-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "firmware.h"
#include "bootlibp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BOOT_HEAP_GRANULARITY 0x1000
#define BOOT_HEAP_EXPANSION_SIZE (0x10 * 0x1000)
#define BOOT_ALLOCATION_TAG 0x746F6F42 // 'tooB'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
BopHandleHeapCorruption (
    PMEMORY_HEAP Heap,
    HEAP_CORRUPTION_CODE Code,
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

MEMORY_DESCRIPTOR_LIST BoMemoryMap;
MEMORY_HEAP BoHeap;

//
// ------------------------------------------------------------------ Functions
//

PVOID
BoAllocateMemory (
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates memory in the loader. This memory is marked as
    loader temporary, meaning it will get unmapped and reclaimed during kernel
    initialization.

Arguments:

    Size - Supplies the size of the desired allocation, in bytes.

Return Value:

    Returns a physical pointer to the allocation on success, or NULL on failure.

--*/

{

    return RtlHeapAllocate(&BoHeap, Size, BOOT_ALLOCATION_TAG);
}

VOID
BoFreeMemory (
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees memory allocated in the boot environment.

Arguments:

    Allocation - Supplies a pointer to the memory allocation being freed.

Return Value:

    None.

--*/

{

    RtlHeapFree(&BoHeap, Allocation);
    return;
}

KSTATUS
BopInitializeMemory (
    PBOOT_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes memory services for the boot library.

Arguments:

    Parameters - Supplies a pointer to the application initialization
        information.

Return Value:

    Status code.

--*/

{

    MEMORY_DESCRIPTOR Descriptor;
    PBOOT_RESERVED_REGION Region;
    ULONG RegionIndex;
    KSTATUS Status;

    //
    // Loop through and mark all the reserved regions to prevent allocations
    // there. Some firmware (PC/AT) doesn't track allocations made by boot
    // applications, and this list is used to mark allocations from a previous
    // boot application (like the boot manager).
    //

    Region = (PVOID)(UINTN)(Parameters->ReservedRegions);
    for (RegionIndex = 0;
         RegionIndex < Parameters->ReservedRegionCount;
         RegionIndex += 1) {

        //
        // Mark these regions as "firmware temporary" so that they can get
        // reclaimed in the kernel, but don't get freed if this boot
        // application fails and cleans up.
        //

        MmMdInitDescriptor(&Descriptor,
                           Region->Address,
                           Region->Address + Region->Size,
                           MemoryTypeFirmwareTemporary);

        Status = MmMdAddDescriptorToList(&BoMemoryMap, &Descriptor);
        if (!KSUCCESS(Status)) {
            goto InitializeMemoryEnd;
        }

        Region += 1;
    }

    RtlHeapInitialize(&BoHeap,
                      BoExpandHeap,
                      NULL,
                      BopHandleHeapCorruption,
                      BOOT_HEAP_EXPANSION_SIZE,
                      BOOT_HEAP_GRANULARITY,
                      0,
                      MEMORY_HEAP_FLAG_NO_PARTIAL_FREES);

    Status = STATUS_SUCCESS;

InitializeMemoryEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BopHandleHeapCorruption (
    PMEMORY_HEAP Heap,
    HEAP_CORRUPTION_CODE Code,
    PVOID Parameter
    )

/*++

Routine Description:

    This routine is called when the heap detects internal corruption.

Arguments:

    Heap - Supplies a pointer to the heap containing the corruption.

    Code - Supplies the code detailing the problem.

    Parameter - Supplies an optional parameter pointing at a problem area.

Return Value:

    None. This routine probably shouldn't return.

--*/

{

    RtlDebugPrint(" *** Heap corruption: Heap 0x%x, Code %d, Parameter 0x%x "
                  "***\n",
                  Heap,
                  Code,
                  Parameter);

    ASSERT(FALSE);

    return;
}


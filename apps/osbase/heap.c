/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    heap.c

Abstract:

    This module implements the memory heap for the C library.

Author:

    Evan Green 6-Mar-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the minimum heap expansion size.
//

#define SYSTEM_HEAP_MINIMUM_EXPANSION_PAGES 0x10
#define SYSTEM_HEAP_MAGIC 0x6C6F6F50 // 'looP'
#define SYSTEM_HEAP_DIRECT_ALLOCATION_THRESHOLD (256 * _1MB)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
OspHeapExpand (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    );

BOOL
OspHeapContract (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    );

VOID
OspHeapCorruption (
    PMEMORY_HEAP Heap,
    HEAP_CORRUPTION_CODE Code,
    PVOID Parameter
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the primary heap.
//

MEMORY_HEAP OsHeap;
OS_LOCK OsHeapLock;

//
// Store the native page shift and mask.
//

UINTN OsPageShift;
UINTN OsPageSize;

//
// ------------------------------------------------------------------ Functions
//

OS_API
PVOID
OsHeapAllocate (
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine allocates memory from the heap.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identifier to associate with the allocation, which aides
        in debugging.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    PVOID Allocation;

    OsAcquireLock(&OsHeapLock);
    Allocation = RtlHeapAllocate(&OsHeap, Size, Tag);
    OsReleaseLock(&OsHeapLock);
    return Allocation;
}

OS_API
VOID
OsHeapFree (
    PVOID Memory
    )

/*++

Routine Description:

    This routine frees memory, making it available for other users of the heap.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

{

    OsAcquireLock(&OsHeapLock);
    RtlHeapFree(&OsHeap, Memory);
    OsReleaseLock(&OsHeapLock);
    return;
}

OS_API
PVOID
OsHeapReallocate (
    PVOID Memory,
    UINTN NewSize,
    UINTN Tag
    )

/*++

Routine Description:

    This routine resizes the given allocation, potentially creating a new
    buffer and copying the old contents in.

Arguments:

    Memory - Supplies the original active allocation. If this parameter is
        NULL, this routine will simply allocate memory.

    NewSize - Supplies the new required size of the allocation. If this is
        0, then the original allocation will simply be freed.

    Tag - Supplies an identifier to associate with the allocation, which aides
        in debugging.

Return Value:

    Returns a pointer to a buffer with the new size (and original contents) on
    success. This may be a new buffer or the same one.

    NULL on failure or if the new size supplied was zero.

--*/

{

    PVOID Allocation;

    OsAcquireLock(&OsHeapLock);
    Allocation = RtlHeapReallocate(&OsHeap, Memory, NewSize, Tag);
    OsReleaseLock(&OsHeapLock);
    return Allocation;
}

OS_API
KSTATUS
OsHeapAlignedAllocate (
    PVOID *Memory,
    UINTN Alignment,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine allocates aligned memory from the heap.

Arguments:

    Memory - Supplies a pointer that receives the pointer to the aligned
        allocation on success.

    Alignment - Supplies the requested alignment for the allocation, in bytes.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies an identification tag to mark this allocation with.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    OsAcquireLock(&OsHeapLock);
    Status = RtlHeapAlignedAllocate(&OsHeap, Memory, Alignment, Size, Tag);
    OsReleaseLock(&OsHeapLock);
    return Status;
}

OS_API
VOID
OsValidateHeap (
    VOID
    )

/*++

Routine Description:

    This routine validates the memory heap for consistency, ensuring that no
    corruption or other errors are present in the heap.

Arguments:

    None.

Return Value:

    None.

--*/

{

    OsAcquireLock(&OsHeapLock);
    RtlValidateHeap(&OsHeap, NULL);
    OsReleaseLock(&OsHeapLock);
    return;
}

VOID
OspInitializeMemory (
    VOID
    )

/*++

Routine Description:

    This routine initializes the memory heap portion of the OS base library.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Flags;

    OsInitializeLockDefault(&OsHeapLock);
    OsPageSize = OsEnvironment->StartData->PageSize;
    OsPageShift = RtlCountTrailingZeros(OsPageSize);
    Flags = MEMORY_HEAP_FLAG_NO_PARTIAL_FREES;
    RtlHeapInitialize(&OsHeap,
                      OspHeapExpand,
                      OspHeapContract,
                      OspHeapCorruption,
                      SYSTEM_HEAP_MINIMUM_EXPANSION_PAGES << OsPageShift,
                      OsPageSize,
                      SYSTEM_HEAP_MAGIC,
                      Flags);

    OsHeap.DirectAllocationThreshold = SYSTEM_HEAP_DIRECT_ALLOCATION_THRESHOLD;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
OspHeapExpand (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine is called when the heap wants to expand and get more space.

Arguments:

    Heap - Supplies a pointer to the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies a 32-bit tag to associate with this allocation for debugging
        purposes. These are usually four ASCII characters so as to stand out
        when a poor developer is looking at a raw memory dump. It could also be
        a return address.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    PVOID Expansion;
    ULONG Flags;
    KSTATUS Status;

    Expansion = NULL;
    Flags = SYS_MAP_FLAG_ANONYMOUS |
            SYS_MAP_FLAG_READ |
            SYS_MAP_FLAG_WRITE;

    Status = OsMemoryMap(INVALID_HANDLE,
                         0,
                         Size,
                         Flags,
                         &Expansion);

    if (!KSUCCESS(Status)) {
        return NULL;
    }

    return Expansion;
}

BOOL
OspHeapContract (
    PMEMORY_HEAP Heap,
    PVOID Memory,
    UINTN Size
    )

/*++

Routine Description:

    This routine is called when the heap wants to release space it had
    previously been allocated.

Arguments:

    Heap - Supplies a pointer to the heap the memory was originally allocated
        from.

    Memory - Supplies the allocation returned by the allocation routine.

    Size - Supplies the number of bytes to release.

Return Value:

    TRUE if the memory was successfully freed.

    FALSE if the memory could not be freed at this time.

--*/

{

    KSTATUS Status;

    Status = OsMemoryUnmap(Memory, Size);
    if (!KSUCCESS(Status)) {

        ASSERT(FALSE);

        return FALSE;
    }

    return TRUE;
}

VOID
OspHeapCorruption (
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

    RtlDebugPrint("\n\n *** Exiting due to heap corruption: code %d "
                  "Parameter 0x%x***\n\n",
                  Code,
                  Parameter);

    OsSendSignal(SignalTargetCurrentProcess,
                 0,
                 SIGNAL_ABORT,
                 0,
                 0);

    return;
}


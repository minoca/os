/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pool.c

Abstract:

    This module implements support for core UEFI pool allocations.

Author:

    Evan Green 28-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro gets the pointer to the pool tail structure from the header.
//

#define POOL_HEADER_TO_TAIL(_Header) \
    ((PPOOL_TAIL)(((CHAR8 *)(_Header)) + (_Header)->Size - sizeof(POOL_TAIL)))

//
// This macro converts the given size in bytes to a pool bucket number.
//

#define POOL_SIZE_TO_LIST(_Size) ((_Size) >> POOL_SHIFT)

//
// This macro converts a bucket index conservatively back into a byte size.
//

#define POOL_LIST_TO_SIZE(_List) (((_List) + 1) << POOL_SHIFT)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the constants put in pool structures to verify validity.
//

#define POOL_MAGIC 0x6C6F6F50 // 'looP'
#define POOL_HEADER_MAGIC 0x6C6F6F50 // 'looP'
#define POOL_FREE_MAGIC 0x65657246 // 'eerF'
#define POOL_TAIL_MAGIC 0x6C696154 // 'liaT'

//
// Define the granularity of the pool buckets.
//

#define POOL_SHIFT 7

//
// Define the total size of the pool overhead.
//

#define POOL_OVERHEAD (sizeof(POOL_HEADER) + sizeof(POOL_TAIL))

//
// Define the number of pool buckets to store before it makes sense to just
// start allocating pages directly.
//

#define MAX_POOL_LIST POOL_SIZE_TO_LIST(EFI_PAGE_SIZE)

//
// Define the level that pool raises to.
//

#define POOL_TPL TPL_NOTIFY

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _POOL_FREE_ENTRY {
    LIST_ENTRY ListEntry;
    UINT32 Magic;
    UINT32 Index;
} POOL_FREE_ENTRY, *PPOOL_FREE_ENTRY;

typedef struct _POOL_HEADER {
    UINT32 Magic;
    UINT32 Padding;
    EFI_MEMORY_TYPE MemoryType;
    UINTN Size;
} POOL_HEADER, *PPOOL_HEADER;

typedef struct _POOL_TAIL {
    UINT32 Magic;
    UINT32 Padding;
    UINTN Size;
} POOL_TAIL, *PPOOL_TAIL;

typedef struct _POOL {
    LIST_ENTRY ListEntry;
    UINTN Magic;
    UINTN UsedSize;
    EFI_MEMORY_TYPE MemoryType;
    LIST_ENTRY FreeList[MAX_POOL_LIST];
} POOL, *PPOOL;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID *
EfipCoreAllocatePool (
    EFI_MEMORY_TYPE PoolType,
    UINTN Size
    );

EFI_STATUS
EfipCoreFreePool (
    VOID *Buffer
    );

PPOOL
EfipCoreLookupPool (
    EFI_MEMORY_TYPE PoolType
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the builtin pools for each memory type.
//

POOL EfiPool[EfiMaxMemoryType];

//
// Store the list of pools to search.
//

LIST_ENTRY EfiPoolList;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfiCoreInitializePool (
    VOID
    )

/*++

Routine Description:

    This routine initializes EFI core pool services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    PPOOL Pool;
    UINTN PoolIndex;
    UINTN PoolListIndex;

    INITIALIZE_LIST_HEAD(&EfiPoolList);
    for (PoolIndex = 0; PoolIndex < EfiMaxMemoryType; PoolIndex += 1) {
        Pool = &(EfiPool[PoolIndex]);
        Pool->Magic = POOL_MAGIC;
        Pool->UsedSize = 0;
        Pool->MemoryType = (EFI_MEMORY_TYPE)PoolIndex;
        for (PoolListIndex = 0;
             PoolListIndex < MAX_POOL_LIST;
             PoolListIndex += 1) {

            INITIALIZE_LIST_HEAD(&(Pool->FreeList[PoolListIndex]));
        }
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreAllocatePool (
    EFI_MEMORY_TYPE PoolType,
    UINTN Size,
    VOID **Buffer
    )

/*++

Routine Description:

    This routine allocates memory from the heap.

Arguments:

    PoolType - Supplies the type of pool to allocate.

    Size - Supplies the number of bytes to allocate from the pool.

    Buffer - Supplies a pointer where a pointer to the allocated buffer will
        be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

    EFI_INVALID_PARAMETER if the pool type was invalid or the buffer is NULL.

--*/

{

    EFI_STATUS Status;

    //
    // Fail invalid types.
    //

    if ((((UINT32)PoolType >= EfiMaxMemoryType) &&
        ((UINT32)PoolType < 0x7FFFFFFF)) ||
        (PoolType == EfiConventionalMemory)) {

        return EFI_INVALID_PARAMETER;
    }

    if (Buffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Status = EfiCoreAcquireLockOrFail(&EfiMemoryLock);
    if (EFI_ERROR(Status)) {
        return EFI_OUT_OF_RESOURCES;
    }

    *Buffer = EfipCoreAllocatePool(PoolType, Size);
    EfiCoreReleaseLock(&EfiMemoryLock);
    if (*Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreFreePool (
    VOID *Buffer
    )

/*++

Routine Description:

    This routine frees heap allocated memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to free.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the buffer was invalid.

--*/

{

    EFI_STATUS Status;

    if (Buffer == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    EfiCoreAcquireLock(&EfiMemoryLock);
    Status = EfipCoreFreePool(Buffer);
    EfiCoreReleaseLock(&EfiMemoryLock);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID *
EfipCoreAllocatePool (
    EFI_MEMORY_TYPE PoolType,
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates memory from a pool.

Arguments:

    PoolType - Supplies the type of pool to allocate.

    Size - Supplies the number of bytes to allocate from the pool.

Return Value:

    Returns a pointer to the allocation on sucess.

    NULL on allocation failure.

--*/

{

    UINTN BlockSize;
    VOID *Buffer;
    PPOOL_FREE_ENTRY FreeEntry;
    PPOOL_HEADER Header;
    UINTN ListIndex;
    CHAR8 *NewPage;
    UINTN Offset;
    UINTN PageCount;
    PPOOL Pool;
    PPOOL_TAIL Tail;

    ASSERT(EfiCoreIsLockHeld(&EfiMemoryLock) != FALSE);

    Size = ALIGN_VARIABLE(Size);
    Size += POOL_OVERHEAD;
    ListIndex = POOL_SIZE_TO_LIST(Size);
    Pool = EfipCoreLookupPool(PoolType);
    if (Pool == NULL) {
        return NULL;
    }

    Header = NULL;

    //
    // If the allocation size is big enough, just allocate pages.
    //

    if (ListIndex >= MAX_POOL_LIST) {
        PageCount = ALIGN_VALUE(EFI_SIZE_TO_PAGES(Size),
                                EFI_SIZE_TO_PAGES(EFI_MEMORY_EXPANSION_SIZE));

        Header = EfiCoreAllocatePoolPages(PoolType,
                                          PageCount,
                                          EFI_MEMORY_EXPANSION_SIZE);

        goto CoreAllocatePoolEnd;
    }

    //
    // If there's no free pool left in the bucket, allocate more pages.
    //

    if (LIST_EMPTY(&(Pool->FreeList[ListIndex])) != FALSE) {
        NewPage = EfiCoreAllocatePoolPages(
                                  PoolType,
                                  EFI_SIZE_TO_PAGES(EFI_MEMORY_EXPANSION_SIZE),
                                  EFI_MEMORY_EXPANSION_SIZE);

        if (NewPage == NULL) {
            goto CoreAllocatePoolEnd;
        }

        //
        // Carve up the new page into free pool blocks. Add as many as possible
        // to the desired list, then successively add the remainders to the
        // smaller lists.
        //

        Offset = 0;
        while (Offset < EFI_MEMORY_EXPANSION_SIZE) {

            ASSERT(ListIndex < MAX_POOL_LIST);

            BlockSize = POOL_LIST_TO_SIZE(ListIndex);
            while (Offset + BlockSize <= EFI_MEMORY_EXPANSION_SIZE) {
                FreeEntry = (PPOOL_FREE_ENTRY)(&(NewPage[Offset]));
                FreeEntry->Magic = POOL_FREE_MAGIC;
                FreeEntry->Index = ListIndex;
                INSERT_BEFORE(&(FreeEntry->ListEntry),
                              &(Pool->FreeList[ListIndex]));

                Offset += BlockSize;
            }

            ListIndex -= 1;
        }

        ASSERT(Offset == EFI_MEMORY_EXPANSION_SIZE);

        ListIndex = POOL_SIZE_TO_LIST(Size);
    }

    //
    // Remove the first free entry from the pool.
    //

    FreeEntry = LIST_VALUE(Pool->FreeList[ListIndex].Next,
                           POOL_FREE_ENTRY,
                           ListEntry);

    LIST_REMOVE(&(FreeEntry->ListEntry));
    Header = (PPOOL_HEADER)FreeEntry;

CoreAllocatePoolEnd:
    Buffer = NULL;
    if (Header != NULL) {

        //
        // Initialize the header and tail information.
        //

        Header->Magic = POOL_HEADER_MAGIC;
        Header->Size = Size;
        Header->MemoryType = PoolType;
        Tail = POOL_HEADER_TO_TAIL(Header);
        Tail->Magic = POOL_TAIL_MAGIC;
        Tail->Size = Size;
        Buffer = Header + 1;
        Pool->UsedSize += Size;
    }

    return Buffer;
}

EFI_STATUS
EfipCoreFreePool (
    VOID *Buffer
    )

/*++

Routine Description:

    This routine frees heap allocated memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to free.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the buffer was invalid.

--*/

{

    PPOOL_FREE_ENTRY FreeEntry;
    PPOOL_HEADER Header;
    UINTN ListIndex;
    UINTN PageCount;
    PPOOL Pool;
    PPOOL_TAIL Tail;

    ASSERT(Buffer != NULL);

    Header = Buffer;
    Header -= 1;

    ASSERT(Header != NULL);

    if (Header->Magic != POOL_HEADER_MAGIC) {

        ASSERT(FALSE);

        return EFI_INVALID_PARAMETER;
    }

    Tail = POOL_HEADER_TO_TAIL(Header);

    ASSERT(Tail != NULL);
    ASSERT(EfiCoreIsLockHeld(&EfiMemoryLock) != FALSE);

    if (Tail->Magic != POOL_TAIL_MAGIC) {

        ASSERT(FALSE);

        return EFI_INVALID_PARAMETER;
    }

    if (Tail->Size != Header->Size) {

        ASSERT(FALSE);

        return EFI_INVALID_PARAMETER;
    }

    //
    // Determine the pool type.
    //

    Pool = EfipCoreLookupPool(Header->MemoryType);
    if (Pool == NULL) {

        ASSERT(FALSE);

        return EFI_INVALID_PARAMETER;
    }

    ASSERT(Pool->UsedSize >= Header->Size);

    Pool->UsedSize -= Header->Size;

    //
    // If the pool is not on any list, free the pages directly.
    //

    ListIndex = POOL_SIZE_TO_LIST(Header->Size);
    if (ListIndex >= MAX_POOL_LIST) {
        PageCount = ALIGN_VALUE(EFI_SIZE_TO_PAGES(Header->Size),
                                EFI_SIZE_TO_PAGES(EFI_MEMORY_EXPANSION_SIZE));

        EfiCoreFreePoolPages((EFI_PHYSICAL_ADDRESS)(UINTN)Header, PageCount);

    //
    // Put the pool entry back onto the free list.
    //

    } else {
        FreeEntry = (PPOOL_FREE_ENTRY)Header;
        FreeEntry->Magic = POOL_FREE_MAGIC;
        FreeEntry->Index = ListIndex;
        INSERT_AFTER(&(FreeEntry->ListEntry), &(Pool->FreeList[ListIndex]));
    }

    return EFI_SUCCESS;
}

PPOOL
EfipCoreLookupPool (
    EFI_MEMORY_TYPE PoolType
    )

/*++

Routine Description:

    This routine finds the pool for the given pool type. If there is none,
    it creates one.

Arguments:

    PoolType - Supplies the type of pool to allocate.

Return Value:

    Returns a pointer to the pool for the specified type on success.

    NULL on allocation failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN FreeListIndex;
    PPOOL Pool;

    //
    // If the memory type is a builtin EFI type, then just return the pool
    // right quick.
    //

    if ((UINT32)PoolType < EfiMaxMemoryType) {
        return &(EfiPool[PoolType]);
    }

    //
    // Root through the existing pools to try to find it.
    //

    CurrentEntry = EfiPoolList.Next;
    while (CurrentEntry != &EfiPoolList) {
        Pool = LIST_VALUE(CurrentEntry, POOL, ListEntry);
        if (Pool->MemoryType == PoolType) {
            return Pool;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // The pool wasn't found, it will need to be created.
    //

    Pool = EfipCoreAllocatePool(PoolType, sizeof(POOL));
    if (Pool == NULL) {
        return NULL;
    }

    Pool->Magic = POOL_MAGIC;
    Pool->UsedSize = 0;
    Pool->MemoryType = PoolType;
    for (FreeListIndex = 0; FreeListIndex < MAX_POOL_LIST; FreeListIndex += 1) {
        INITIALIZE_LIST_HEAD(&(Pool->FreeList[FreeListIndex]));
    }

    INSERT_BEFORE(&(Pool->ListEntry), &EfiPoolList);
    return Pool;
}


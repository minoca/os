/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    util.c

Abstract:

    This module implements miscellaneous utility functions for the Chalk
    interpreter.

Author:

    Evan Green 20-Nov-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CHALK_ALLOCATION_MAGIC 0x41414141

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CHALK_ALLOCATION {
    ULONG Magic;
    LIST_ENTRY ListEntry;
    PVOID Caller;
    ULONG Size;
} CHALK_ALLOCATION, *PCHALK_ALLOCATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

UINTN ChalkAllocations = 0;

LIST_ENTRY ChalkAllocationList;
BOOL ChalkDebugLeaks = FALSE;

//
// ------------------------------------------------------------------ Functions
//

PVOID
ChalkAllocate (
    size_t Size
    )

/*++

Routine Description:

    This routine dynamically allocates memory for the Chalk interpreter.

Arguments:

    Size - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the memory on success.

    NULL on allocation failure.

--*/

{

    PVOID Allocation;
    PCHALK_ALLOCATION ChalkAllocation;

    if (ChalkDebugLeaks != FALSE) {
        Size += sizeof(CHALK_ALLOCATION);
        if (ChalkAllocationList.Next == NULL) {
            INITIALIZE_LIST_HEAD(&ChalkAllocationList);
        }
    }

    Allocation = malloc(Size);
    if (Allocation != NULL) {
        ChalkAllocations += 1;
        if (ChalkDebugLeaks != FALSE) {
            ChalkAllocation = Allocation;
            ChalkAllocation->Magic = CHALK_ALLOCATION_MAGIC;

            //
            // Set the caller to something like __builtin_return_address(0) to
            // figure out where the allocation came from.
            //

            ChalkAllocation->Caller = NULL;
            ChalkAllocation->Size = Size - sizeof(CHALK_ALLOCATION);
            Allocation = ChalkAllocation + 1;
            INSERT_BEFORE(&(ChalkAllocation->ListEntry), &ChalkAllocationList);
        }
    }

    return Allocation;
}

PVOID
ChalkReallocate (
    PVOID Allocation,
    size_t Size
    )

/*++

Routine Description:

    This routine reallocates a previously allocated dynamic memory chunk,
    changing its size.

Arguments:

    Allocation - Supplies the previous allocation.

    Size - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the memory on success.

    NULL on allocation failure.

--*/

{

    PCHALK_ALLOCATION ChalkAllocation;
    PVOID NewAllocation;

    if (Allocation == NULL) {
        return ChalkAllocate(Size);
    }

    if (ChalkDebugLeaks == FALSE) {
        return realloc(Allocation, Size);
    }

    //
    // In debug mode, it's easier to just grab a new allocation and free the
    // old one.
    //

    ChalkAllocation = Allocation;
    ChalkAllocation -= 1;

    assert(ChalkAllocation->Magic == CHALK_ALLOCATION_MAGIC);

    NewAllocation = ChalkAllocate(Size);
    if (NewAllocation == NULL) {
        return NULL;
    }

    memcpy(NewAllocation, Allocation, ChalkAllocation->Size);
    ChalkFree(Allocation);
    return NewAllocation;
}

VOID
ChalkFree (
    PVOID Allocation
    )

/*++

Routine Description:

    This routine releases dynamically allocated memory back to the system.

Arguments:

    Allocation - Supplies a pointer to the allocation received from the
        allocate function.

Return Value:

    None.

--*/

{

    PCHALK_ALLOCATION ChalkAllocation;

    assert(Allocation != NULL);
    assert(ChalkAllocations != 0);

    if (ChalkDebugLeaks != FALSE) {
        ChalkAllocation = Allocation;
        ChalkAllocation -= 1;
        Allocation = ChalkAllocation;

        assert(ChalkAllocation->Magic == CHALK_ALLOCATION_MAGIC);

        LIST_REMOVE(&(ChalkAllocation->ListEntry));
    }

    ChalkAllocations -= 1;
    free(Allocation);
    return;
}

VOID
ChalkPrintAllocations (
    VOID
    )

/*++

Routine Description:

    This routine prints any outstanding Chalk allocations.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PCHALK_ALLOCATION Allocation;
    PLIST_ENTRY CurrentEntry;

    if (ChalkAllocations != 0) {
        printf("%ld allocations\n", ChalkAllocations);
    }

    if (ChalkDebugLeaks != FALSE) {
        CurrentEntry = ChalkAllocationList.Next;
        while (CurrentEntry != &ChalkAllocationList) {
            Allocation = LIST_VALUE(CurrentEntry, CHALK_ALLOCATION, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            printf("Allocation %p Size %x Caller %p\n",
                   Allocation + 1,
                   Allocation->Size,
                   Allocation->Caller);
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


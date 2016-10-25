/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    heap.c

Abstract:

    This module implements heap functionality for the C Library.

Author:

    Evan Green 6-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MALLOC_ALLOCATION_TAG 0x6C6C614D // 'llaM'

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
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
free (
    void *Memory
    )

/*++

Routine Description:

    This routine frees previously allocated memory.

Arguments:

    Memory - Supplies a pointer to memory returned by the allocation function.

Return Value:

    None.

--*/

{

    if (Memory == NULL) {
        return;
    }

    OsHeapFree(Memory);
    return;
}

LIBC_API
void *
malloc (
    size_t AllocationSize
    )

/*++

Routine Description:

    This routine allocates memory from the heap.

Arguments:

    AllocationSize - Supplies the required allocation size in bytes.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on failure.

--*/

{

    if (AllocationSize == 0) {
        AllocationSize = 1;
    }

    return OsHeapAllocate(AllocationSize, MALLOC_ALLOCATION_TAG);
}

LIBC_API
void *
realloc (
    void *Allocation,
    size_t AllocationSize
    )

/*++

Routine Description:

    This routine resizes the given buffer. If the new allocation size is
    greater than the original allocation, the contents of the new bytes are
    unspecified (just like the contents of a malloced buffer).

Arguments:

    Allocation - Supplies the optional original allocation. If this is not
        supplied, this routine is equivalent to malloc.

    AllocationSize - Supplies the new required allocation size in bytes.

Return Value:

    Returns a pointer to the new buffer on success.

    NULL on failure or if the supplied size was zero. If the new buffer could
    not be allocated, errno will be set to ENOMEM.

--*/

{

    void *NewBuffer;

    if ((Allocation == NULL) && (AllocationSize == 0)) {
        AllocationSize = 1;
    }

    NewBuffer = OsHeapReallocate(Allocation,
                                 AllocationSize,
                                 MALLOC_ALLOCATION_TAG);

    if ((NewBuffer == NULL) && (AllocationSize != 0)) {
        errno = ENOMEM;
    }

    return NewBuffer;
}

LIBC_API
void *
calloc (
    size_t ElementCount,
    size_t ElementSize
    )

/*++

Routine Description:

    This routine allocates memory from the heap large enough to store the
    given number of elements of the given size (the product of the two
    parameters). The buffer returned will have all zeros written to it.

Arguments:

    ElementCount - Supplies the number of elements to allocate.

    ElementSize - Supplies the size of each element in bytes.

Return Value:

    Returns a pointer to the new zeroed buffer on success.

    NULL on failure or if the either the element count of the element size was
    zero. If the new buffer could not be allocated, errno will be set to ENOMEM.

--*/

{

    void *NewBuffer;
    size_t TotalSize;

    TotalSize = ElementCount * ElementSize;
    if ((ElementCount != 0) && ((TotalSize / ElementCount) != ElementSize)) {
        errno = ENOMEM;
        return NULL;
    }

    if (TotalSize == 0) {
        TotalSize = 1;
    }

    NewBuffer = OsHeapAllocate(TotalSize, MALLOC_ALLOCATION_TAG);
    if (NewBuffer == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    memset(NewBuffer, 0, TotalSize);
    return NewBuffer;
}

LIBC_API
int
posix_memalign (
    void **AllocationPointer,
    size_t AllocationAlignment,
    size_t AllocationSize
    )

/*++

Routine Description:

    This routine allocates aligned memory from the heap. The given alignment
    must be a power of 2 and a multiple of the size of a pointer.

Arguments:

    AllocationPointer - Supplies a pointer that receives a pointer to the
        allocated memory on success.

    AllocationAlignment - Supplies the required allocation alignment in bytes.

    AllocationSize - Supplies the required allocation size in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    KSTATUS Status;

    if ((IS_ALIGNED(AllocationAlignment, sizeof(void *)) == FALSE) ||
        (POWER_OF_2(AllocationAlignment) == FALSE) ||
        (AllocationAlignment == 0)) {

        return EINVAL;
    }

    Status = OsHeapAlignedAllocate(AllocationPointer,
                                   AllocationAlignment,
                                   AllocationSize,
                                   MALLOC_ALLOCATION_TAG);

    return ClConvertKstatusToErrorNumber(Status);
}

//
// --------------------------------------------------------- Internal Functions
//


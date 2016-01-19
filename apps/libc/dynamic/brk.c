/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    brk.c

Abstract:

    This module implements support for the legacy brk and sbrk functions.

Author:

    Evan Green 29-Apr-2014

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define INITIAL_BREAK_SIZE 0x10000

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
// Store the current break.
//

void *ClBreakBase;
void *ClCurrentBreak;
size_t ClBreakSize;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
brk (
    void *Address
    )

/*++

Routine Description:

    This routine sets the current program break to the specified address.

    New programs should use malloc and free in favor of this deprecated
    legacy function. This function is likely to fail if any other memory
    functions such as malloc or free are used. Other functions, including the
    C library, may use malloc and free silently. This function is neither
    thread-safe nor reentrant.

Arguments:

    Address - Supplies the new address of the program break.

Return Value:

    0 on success.

    -1 on failure, and errno is set to indicate the error.

--*/

{

    size_t AdditionalSize;
    void *ExtraSpace;
    ULONG Flags;
    size_t NewSize;
    KSTATUS Status;

    Flags = SYS_MAP_FLAG_ANONYMOUS |
            SYS_MAP_FLAG_READ |
            SYS_MAP_FLAG_WRITE |
            SYS_MAP_FLAG_EXECUTE;

    if (ClBreakBase == NULL) {
        Status = OsMemoryMap(INVALID_HANDLE,
                             0,
                             INITIAL_BREAK_SIZE,
                             Flags,
                             &ClBreakBase);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        ClBreakSize = INITIAL_BREAK_SIZE;
        ClCurrentBreak = ClBreakBase;
    }

    if ((size_t)Address < (size_t)ClBreakBase) {
        errno = EINVAL;
        return -1;
    }

    NewSize = ALIGN_RANGE_UP((size_t)Address - (size_t)ClBreakBase, 0x1000);
    if (NewSize < ClBreakSize) {
        ClCurrentBreak = Address;
        return 0;
    }

    AdditionalSize = NewSize - ClBreakSize;
    ExtraSpace = ClBreakBase + ClBreakSize;
    if (AdditionalSize != 0) {
        Status = OsMemoryMap(INVALID_HANDLE,
                             0,
                             AdditionalSize,
                             Flags,
                             &ExtraSpace);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        if (ExtraSpace != ClBreakBase + ClBreakSize) {
            OsMemoryUnmap(ExtraSpace, AdditionalSize);
            errno = ENOMEM;
            return -1;
        }
    }

    ClBreakSize = NewSize;
    ClCurrentBreak = Address;
    return 0;
}

LIBC_API
void *
sbrk (
    intptr_t Increment
    )

/*++

Routine Description:

    This routine increments the current program break by the given number of
    bytes. If the value is negative, the program break is decreased.

    New programs should use malloc and free in favor of this deprecated
    legacy function. This function is likely to fail if any other memory
    functions such as malloc or free are used. Other functions, including the
    C library, may use malloc and free silently. This function is neither
    thread-safe nor reentrant.

Arguments:

    Increment - Supplies the amount to add or remove from the program break.

Return Value:

    Returns the original program break address before this function changed it
    on success.

    (void *)-1 on failure, and errno is set to indicate the error.

--*/

{

    ULONG Flags;
    void *OriginalBreak;
    size_t OriginalSize;
    int Result;
    KSTATUS Status;

    Flags = SYS_MAP_FLAG_ANONYMOUS |
            SYS_MAP_FLAG_READ |
            SYS_MAP_FLAG_WRITE |
            SYS_MAP_FLAG_EXECUTE;

    if (ClBreakBase == NULL) {
        Status = OsMemoryMap(INVALID_HANDLE,
                             0,
                             INITIAL_BREAK_SIZE,
                             Flags,
                             &ClBreakBase);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return (void *)-1;
        }

        ClBreakSize = INITIAL_BREAK_SIZE;
        ClCurrentBreak = ClBreakBase;
    }

    OriginalSize = (size_t)ClCurrentBreak - (size_t)ClBreakBase;
    if (Increment < 0) {
        if (-Increment > OriginalSize) {
            Increment = -OriginalSize;
        }
    }

    OriginalBreak = ClCurrentBreak;
    Result = brk(ClCurrentBreak + Increment);
    if (Result != 0) {
        return (void *)-1;
    }

    return OriginalBreak;
}

//
// --------------------------------------------------------- Internal Functions
//


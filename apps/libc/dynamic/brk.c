/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

void *ClCurrentBreak;

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

    PVOID CurrentBreak;

    CurrentBreak = OsSetProgramBreak(Address);
    ClCurrentBreak = CurrentBreak;
    if (CurrentBreak != Address) {
        errno = ENOMEM;
        return -1;
    }

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

    PVOID OldBreak;

    if (ClCurrentBreak == NULL) {
        ClCurrentBreak = OsSetProgramBreak(NULL);
    }

    OldBreak = ClCurrentBreak;
    if (Increment == 0) {
        return OldBreak;
    }

    if (brk(OldBreak + Increment) != 0) {
        return (void *)-1;
    }

    return OldBreak;
}

//
// --------------------------------------------------------- Internal Functions
//


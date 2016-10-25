/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    aatexit.c

Abstract:

    This module implements the ARM-specific __aeabi_atexit function, which
    simply turns around and calls __cxa_atexit.

Author:

    Evan Green 5-May-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

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

__HIDDEN
int
__aeabi_atexit (
    void *Argument,
    void (*DestructorFunction)(void *),
    void *SharedObject
    )

/*++

Routine Description:

    This routine is called to register a global static destructor function on
    ARM.

Arguments:

    Argument - Supplies an argument to pass the function when it is called.

    DestructorFunction - Supplies a pointer to the function to call.

    SharedObject - Supplies a pointer to the shared object this destructor is
        associated with.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return __cxa_atexit((void *)DestructorFunction, Argument, SharedObject);
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    atexit.c

Abstract:

    This module implements the atexit routine, which is implemented as a
    static function so that the calling module of the atexit registration
    can be identified.

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
// There exists a variable that will have a unique address in every dynamic
// object. This is simply a global marked "hidden", and is defined within
// GCC's crtstuff.c file.
//

extern void *__dso_handle;

//
// ------------------------------------------------------------------ Functions
//

__HIDDEN
int
atexit (
    void (*ExitFunction)(void)
    )

/*++

Routine Description:

    This routine registers a function to be called when the process exits
    normally via a call to exit or a return from main. Calls to exec clear
    the list of registered exit functions. This routine may allocate memory.
    Functions are called in the reverse order in which they were registered.
    If this function is called from within a shared library, then the given
    function will be called when the library is unloaded.

Arguments:

    ExitFunction - Supplies a pointer to the function to call when the
        process exits normally or the shared object is unloaded.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    return __cxa_atexit((void *)ExitFunction, NULL, &__dso_handle);
}

//
// --------------------------------------------------------- Internal Functions
//


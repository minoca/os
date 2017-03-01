/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dlsym.c

Abstract:

    This module implements the dlsym dynamic library function. This is
    implemented in the C static library in order to support RTLD_NEXT, which
    needs to identify the shared object from which dlsym is called.

Author:

    Evan Green 1-Mar-2017

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <dlfcn.h>

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
void *
dlsym (
    void *Handle,
    const char *SymbolName
    )

/*++

Routine Description:

    This routine returns the address of a symbol defined within an object made
    accessible through a call to dlopen. This routine searches both this object
    and any objects loaded as a result of this one.

Arguments:

    Handle - Supplies a pointer to the opaque handle returned by the dlopen
        routine. Additionally, supply RTLD_DEFAULT to search through the
        executable (global) scope. Supply RTLD_NEXT to search for the next
        instance of the symbol after instance defined in the module that
        called dlsym.

    SymbolName - Supplies a pointer to a null-terminated string containing the
        name of the symbol whose address should be retrieved.

Return Value:

    Returns the address of the symbol on success.

    NULL if the handle was not valid or the symbol could not be found. More
    information can be retrieved via the dlerror function.

--*/

{

    return __dlsym(Handle, SymbolName, &__dso_handle);
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    extsup.c

Abstract:

    This module implements OS-specific support routines for using debugger
    extensions.

Author:

    Evan Green 27-May-2013

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/debug/dbgext.h>

//
// ---------------------------------------------------------------- Definitions
//

#define HANDLE_TABLE_GROWTH 10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PVOID *DbgHandleTable = NULL;
ULONG DbgHandleTableSize = 0;
ULONG DbgNextHandle = 1;

//
// ------------------------------------------------------------------ Functions
//

ULONG
DbgLoadLibrary (
    PSTR BinaryName
    )

/*++

Routine Description:

    This routine loads a shared library.

Arguments:

    BinaryName - Supplies the name of the binary to load.

Return Value:

    Returns a non-zero handle on success.

    0 on failure.

--*/

{

    PVOID NewHandle;
    PVOID *NewHandleTable;

    //
    // Attempt to load the library. Bail on failure.
    //

    NewHandle = dlopen(BinaryName, RTLD_NOW | RTLD_GLOBAL);
    if (NewHandle == NULL) {
        return 0;
    }

    //
    // Save the new handle in the table and return the index representing that
    // handle. If saving this new handle overruns the current buffer, allocate
    // a bigger buffer, copy the old contents in, and free the old buffer.
    //

    if (DbgNextHandle >= DbgHandleTableSize) {
        NewHandleTable = malloc((DbgHandleTableSize + HANDLE_TABLE_GROWTH) *
                                sizeof(PVOID));

        if (NewHandleTable == NULL) {
            dlclose(NewHandle);
            return 0;
        }

        if (DbgHandleTable != NULL) {
            memcpy(NewHandleTable,
                   DbgHandleTable,
                   DbgHandleTableSize * sizeof(PVOID));

            free(DbgHandleTable);
        }

        DbgHandleTable = NewHandleTable;
        DbgHandleTableSize += HANDLE_TABLE_GROWTH;
    }

    assert(DbgNextHandle < DbgHandleTableSize);

    DbgHandleTable[DbgNextHandle] = NewHandle;
    DbgNextHandle += 1;
    return DbgNextHandle - 1;
}

VOID
DbgFreeLibrary (
    ULONG Handle
    )

/*++

Routine Description:

    This routine unloads a shared library.

Arguments:

    Handle - Supplies the handle to to the loaded library.

Return Value:

    None.

--*/

{

    dlclose(DbgHandleTable[Handle]);
    return;
}

PVOID
DbgGetProcedureAddress (
    ULONG Handle,
    PSTR ProcedureName
    )

/*++

Routine Description:

    This routine gets the address of a routine in a loaded shared library (DLL).

Arguments:

    Handle - Supplies the handle to to the loaded library.

    ProcedureName - Supplies the name of the procedure to look up.

Return Value:

    Returns a pointer to the procedure on success.

    NULL on failure.

--*/

{

    return dlsym(DbgHandleTable[Handle], ProcedureName);
}

//
// --------------------------------------------------------- Internal Functions
//


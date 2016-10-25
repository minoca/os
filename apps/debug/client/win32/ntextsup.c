/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntextsup.c

Abstract:

    This module implements OS-specific support routines for using debugger
    extensions on Windows NT.

Author:

    Evan Green 10-Sep-2012

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <windows.h>
#include <stdio.h>
#include <assert.h>

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

HMODULE *DbgHandleTable = NULL;
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

    HMODULE NewHandle;
    HMODULE *NewHandleTable;

    //
    // Attempt to load the library. Bail on failure.
    //

    NewHandle = LoadLibrary(BinaryName);
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
                                sizeof(HMODULE));

        if (NewHandleTable == NULL) {
            FreeLibrary(NewHandle);
            return 0;
        }

        if (DbgHandleTable != NULL) {
            RtlCopyMemory(NewHandleTable,
                          DbgHandleTable,
                          DbgHandleTableSize * sizeof(HMODULE));

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

    FreeLibrary(DbgHandleTable[Handle]);
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

    return GetProcAddress(DbgHandleTable[Handle], ProcedureName);
}

//
// --------------------------------------------------------- Internal Functions
//


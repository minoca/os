/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

#include <stdio.h>
#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include "dbgext.h"

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

    DbgOut("Error: Loading of extensions not yet supported!\n");
    return 0;
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

    DbgOut("Error: Unloading of extensions not yet supported!\n");
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

    DbgOut("Error: DbgGetProcedureAddress not yet supported!\n");
    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//


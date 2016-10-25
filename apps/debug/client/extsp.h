/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    extsp.h

Abstract:

    This header contains private definitions for debugger extension support.

Author:

    Evan Green 10-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define MALLOC(_x) malloc(_x)
#define FREE(_x) free(_x)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
DbgInitializeExtensions (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes support for debugger extensions and loads any
    extensions supplied on the command line.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgLoadExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName
    );

/*++

Routine Description:

    This routine loads a debugger extension library.

Arguments:

    Context - Supplies a pointer to the application context.

    BinaryName - Supplies the path to the binary to load.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

VOID
DbgUnloadExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR BinaryName
    );

/*++

Routine Description:

    This routine unloads and frees a debugger extension library.

Arguments:

    Context - Supplies a pointer to the application context.

    BinaryName - Supplies the path to the binary to unload.

Return Value:

    None.

--*/

VOID
DbgUnloadAllExtensions (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine unloads all debugger extensions.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

INT
DbgDispatchExtension (
    PDEBUGGER_CONTEXT Context,
    PSTR *Arguments,
    ULONG ArgumentCount
    );

/*++

Routine Description:

    This routine dispatches a debugger extension command.

Arguments:

    Context - Supplies a pointer to the application context.

    Arguments - Supplies an array of strings containing the arguments. The
        first argument is the command itself.

    ArgumentCount - Supplies the count of arguments. This is always at least
        one.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

ULONG
DbgLoadLibrary (
    PSTR BinaryName
    );

/*++

Routine Description:

    This routine loads a shared library.

Arguments:

    BinaryName - Supplies the name of the binary to load.

Return Value:

    Returns a non-zero handle on success.

    0 on failure.

--*/

VOID
DbgFreeLibrary (
    ULONG Handle
    );

/*++

Routine Description:

    This routine unloads a shared library.

Arguments:

    Handle - Supplies the handle to to the loaded library.

Return Value:

    None.

--*/

PVOID
DbgGetProcedureAddress (
    ULONG Handle,
    PSTR ProcedureName
    );

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


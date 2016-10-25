/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    vmsys.h

Abstract:

    This header contains definitions for the default configuration that wires
    Chalk up to the rest of the system.

Author:

    Evan Green 28-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern CK_CONFIGURATION CkDefaultConfiguration;
extern PCSTR CkSharedLibraryExtension;

//
// -------------------------------------------------------- Function Prototypes
//

PVOID
CkpLoadLibrary (
    PSTR BinaryName
    );

/*++

Routine Description:

    This routine loads a shared library.

Arguments:

    BinaryName - Supplies the name of the binary to load.

Return Value:

    Returns a handle to the library on success.

    NULL on failure.

--*/

VOID
CkpFreeLibrary (
    PVOID Handle
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
CkpGetLibrarySymbol (
    PVOID Handle,
    PSTR SymbolName
    );

/*++

Routine Description:

    This routine gets the address of a named symbol in a loaded shared library.

Arguments:

    Handle - Supplies the handle to to the loaded library.

    SymbolName - Supplies the name of the symbol to look up.

Return Value:

    Returns a pointer to the symbol (usually a function) on success.

    NULL on failure.

--*/


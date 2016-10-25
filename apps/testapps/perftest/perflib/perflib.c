/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    perflib.c

Abstract:

    This module implements a stub dynamic library for the performance
    benchmark tests to load and unload.

Author:

    Chris Stevens 7-May-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define PTLIB_API __attribute__ ((visibility ("default")))

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
// Stores whether or not the library has been initialized.
//

int PtLibraryInitialized;

//
// ------------------------------------------------------------------ Functions
//

PTLIB_API
void
PtLibraryInitialize (
    void
    )

/*++

Routine Description:

    This routine initializes the stub performance test library.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PtLibraryInitialized = 1;
    return;
}

PTLIB_API
int
PtIsLibraryInitialized (
    void
    )

/*++

Routine Description:

    This routine determines whether or not the library is initialized.

Arguments:

    None.

Return Value:

    Returns 1 if the library has been initialized, or 0 otherwise.

--*/

{

    return PtLibraryInitialized;
}

//
// --------------------------------------------------------- Internal Functions
//


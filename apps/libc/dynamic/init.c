/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    init.c

Abstract:

    This module implements initialization of the Minoca C Library.

Author:

    Evan Green 4-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"

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

LIBC_API
void
ClInitialize (
    void *Environment
    )

/*++

Routine Description:

    This routine initializes the C Runtime library. This routine is normally
    called by statically linked assembly within a program, and unless developing
    outside the usual paradigm should not need to call this routine directly.

Arguments:

    Environment - Supplies a pointer to the environment information to be passed
        on to the OS Base library.

Return Value:

    None.

--*/

{

    OsInitializeLibrary(Environment);
    ClpInitializeEnvironment();
    ClpInitializeTimeZoneSupport();
    ClpInitializeFileIo();
    ClpInitializeSignals();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


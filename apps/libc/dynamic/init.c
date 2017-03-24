/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

__CONSTRUCTOR
void
ClpInitialize (
    void
    )

/*++

Routine Description:

    This routine initializes the C Runtime library. This routine is normally
    called by statically linked assembly within a program, and unless developing
    outside the usual paradigm should not need to call this routine directly.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ClpInitializeEnvironment();
    ClpInitializeTimeZoneSupport();
    ClpInitializeFileIo();
    ClpInitializeSignals();
    ClpInitializeTypeConversions();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


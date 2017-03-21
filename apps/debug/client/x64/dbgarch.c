/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgarch.c

Abstract:

    This module implements architecture specific support for the debugger
    client.

Author:

    Evan Green 20-Mar-2017

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include <minoca/debug/dbgext.h>

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
DbgGetHostMachineType (
    VOID
    )

/*++

Routine Description:

    This routine returns the machine type for the currently running host (this
    machine).

Arguments:

    None.

Return Value:

    Returns the machine type. See MACHINE_TYPE_* definitions.

--*/

{

    return MACHINE_TYPE_X64;
}

//
// --------------------------------------------------------- Internal Functions
//


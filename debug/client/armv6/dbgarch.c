/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    dbgarch.c

Abstract:

    This module implements architecture specific support for the debugger
    client.

Author:

    Chris Stevens 2-Feb-2014

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
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

    return MACHINE_TYPE_ARMV6;
}

//
// --------------------------------------------------------- Internal Functions
//


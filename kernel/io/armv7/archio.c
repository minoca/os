/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    archio.c

Abstract:

    This module implements ARM architecture specific code for the I/O
    Subsystem.

Author:

    Evan Green 15-Dec-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "../iop.h"

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

KSTATUS
IopArchInitializeKnownArbiterRegions (
    VOID
    )

/*++

Routine Description:

    This routine performs any architecture-specific initialization of the
    resource arbiters.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//


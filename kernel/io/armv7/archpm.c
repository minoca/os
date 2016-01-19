/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    archpm.c

Abstract:

    This module implements architecture-specific Power Management Library
    routines.

Author:

    Evan Green 25-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "../pmp.h"

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
PmpArchInitialize (
    VOID
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization for the power
    management library.

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


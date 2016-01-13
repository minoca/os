/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    hmodarch.c

Abstract:

    This module implements architecture specific functionality for the
    hardware module API.

Author:

    Evan Green 31-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "../hlp.h"

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
HlpModArchInitialize (
    ULONG Phase
    )

/*++

Routine Description:

    This routine implements architecture specific initialization for the
    hardware module library.

Arguments:

    Phase - Supplies the initialization phase. The only valid value is currently
        0, which is pre-debugger initialization.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//


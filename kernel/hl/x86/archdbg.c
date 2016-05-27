/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    archdbg.c

Abstract:

    This module implements architecture-specific debug device support for the
    hardware library.

Author:

    Evan Green 8-Apr-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
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
// Builtin hardware module function prototypes.
//

VOID
HlpNs16550SerialModuleEntry (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Built-in hardware modules.
//

PHARDWARE_MODULE_ENTRY HlBuiltinDebugDevices[] = {
    HlpNs16550SerialModuleEntry,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpArchInitializeDebugDevices (
    VOID
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization for the serial
        subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PHARDWARE_MODULE_ENTRY ModuleEntry;
    ULONG ModuleIndex;
    KSTATUS Status;

    //
    // Loop through and initialize every built in hardware module.
    //

    ModuleIndex = 0;
    while (HlBuiltinDebugDevices[ModuleIndex] != NULL) {
        ModuleEntry = HlBuiltinDebugDevices[ModuleIndex];
        ModuleEntry();
        ModuleIndex += 1;
    }

    Status = STATUS_SUCCESS;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//


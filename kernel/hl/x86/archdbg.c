/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

    ULONG ModuleCount;
    PHARDWARE_MODULE_ENTRY ModuleEntry;
    ULONG ModuleIndex;

    //
    // Loop through and initialize every built in hardware module.
    //

    ModuleCount = sizeof(HlBuiltinDebugDevices) /
                  sizeof(HlBuiltinDebugDevices[0]);

    for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
        ModuleEntry = HlBuiltinDebugDevices[ModuleIndex];
        ModuleEntry();
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//


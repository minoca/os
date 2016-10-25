/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archcach.c

Abstract:

    This module implements architecture-specific cache support for the hardware
    library.

Author:

    Chris Stevens 13-Jan-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include "../hlp.h"
#include "../cache.h"

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
HlpOmap4CacheControllerModuleEntry (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Built-in hardware modules.
//

PHARDWARE_MODULE_ENTRY HlBuiltinCacheModules[] = {
    HlpOmap4CacheControllerModuleEntry,
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpArchInitializeCacheControllers (
    VOID
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization for the cache
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
    // On the boot processor, perform one-time initialization.
    //

    if (KeGetCurrentProcessorNumber() == 0) {

        //
        // Loop through and initialize every built in hardware module.
        //

        ModuleCount = sizeof(HlBuiltinCacheModules) /
                      sizeof(HlBuiltinCacheModules[0]);

        for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
            ModuleEntry = HlBuiltinCacheModules[ModuleIndex];
            ModuleEntry();
        }
    }

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//


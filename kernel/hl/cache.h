/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cache.h

Abstract:

    This header contains definitions for the cache library of the hardware
    layer subsystem. These definitions are internal to the hardware library.

Author:

    Chris Stevens 13-Jan-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define cache controller flags.
//

//
// This flag is set if the cache controller has been initialized.
//

#define CACHE_CONTROLLER_FLAG_INITIALIZED 0x00000001

//
// This flag is set if the initialization failed.
//

#define CACHE_CONTROLLER_FLAG_FAILED 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of registered cache controllers.
//

extern LIST_ENTRY HlCacheControllers;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
HlpCacheControllerRegisterHardware (
    PCACHE_CONTROLLER_DESCRIPTION CacheDescription
    );

/*++

Routine Description:

    This routine is called to register a new cache controller with the system.

Arguments:

    CacheDescription - Supplies a pointer to a structure describing the new
        cache controller.

Return Value:

    Status code.

--*/

KSTATUS
HlpArchInitializeCacheControllers (
    VOID
    );

/*++

Routine Description:

    This routine performs architecture-specific initialization for the cache
    subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/


/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgparch.c

Abstract:

    This module contains architecture specific debug port routines.

Author:

    Evan Green 31-Mar-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "firmware.h"
#include "bootlib.h"
#include "loader.h"

//
// --------------------------------------------------------------------- Macros
//

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

VOID
BopDisableLegacyInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine shuts off any legacy interrupts routed to SMIs for boot
    services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
BopExploreForDebugDevice (
    PDEBUG_PORT_TABLE2 *CreatedTable
    )

/*++

Routine Description:

    This routine performs architecture-specific actions to go hunting for a
    debug device.

Arguments:

    CreatedTable - Supplies a pointer where a pointer to a generated debug
        port table will be returned on success.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//


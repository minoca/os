/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootlibp.h

Abstract:

    This header contains internal definitions for the Boot Library. Consumers
    outside the library itself should not include this header.

Author:

    Evan Green 19-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "bootlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
BopInitializeMemory (
    PBOOT_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine initializes memory services for the boot library.

Arguments:

    Parameters - Supplies a pointer to the application initialization
        information.

Return Value:

    Status code.

--*/


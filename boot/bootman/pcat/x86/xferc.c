/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    xferc.c

Abstract:

    This module implements the trampoline that transfers control to another
    32-bit boot application.

Author:

    Evan Green 31-May-2017

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "firmware.h"
#include "bootlib.h"
#include "../../bootman.h"

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

INT
BmpFwTransferToBootApplication (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_APPLICATION_ENTRY EntryPoint
    )

/*++

Routine Description:

    This routine transfers control to another boot application.

Arguments:

    Parameters - Supplies a pointer to the initialization block.

    EntryPoint - Supplies tne address of the entry point routine of the new
        application.

Return Value:

    Returns the integer return value from the application. Often does not
    return on success.

--*/

{

    INT Result;

    if ((Parameters->Flags & BOOT_INITIALIZATION_FLAG_64BIT) != 0) {
        FwPrintString(0,
                      0,
                      "Cannot launch 64-bit loader with 32-bit boot manager");

        return STATUS_NOT_CONFIGURED;
    }

    Result = EntryPoint(Parameters);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//


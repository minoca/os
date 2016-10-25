/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archlib.c

Abstract:

    This module implements the UEFI architecture support library C routines.

Author:

    Evan Green 27-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

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
EfiCoreInvalidateInstructionCacheRange (
    VOID *Address,
    UINTN Length
    )

/*++

Routine Description:

    This routine invalidates a region of memory in the instruction cache.

Arguments:

    Address - Supplies the address to invalidate. If translation is enabled,
        this is a virtual address.

    Length - Supplies the number of bytes in the region to invalidate.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


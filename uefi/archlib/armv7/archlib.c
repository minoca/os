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
// Internal assembly routines
//

VOID
EfipInvalidateInstructionCache (
    VOID
    );

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

    EfipInvalidateInstructionCache();
    return;
}

UINT8
EfiIoPortIn8 (
    UINT16 Address
    )

/*++

Routine Description:

    This routine performs an 8-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

{

    return 0;
}

VOID
EfiIoPortOut8 (
    UINT16 Address,
    UINT8 Value
    )

/*++

Routine Description:

    This routine performs an 8-bit write to the given I/O port.

Arguments:

    Address - Supplies the address to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


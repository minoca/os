/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    regacces.c

Abstract:

    This module implements basic register access functionality.

Author:

    Evan Green 31-Oct-2012

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

UINT32
EfiReadRegister32 (
    VOID *RegisterAddress
    )

/*++

Routine Description:

    This routine performs a 32-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

{

    EfiMemoryBarrier();
    return *((UINT32 *)RegisterAddress);
}

VOID
EfiWriteRegister32 (
    VOID *RegisterAddress,
    UINT32 Value
    )

/*++

Routine Description:

    This routine performs a 32-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    EfiMemoryBarrier();
    *((UINT32 *)RegisterAddress) = Value;
    EfiMemoryBarrier();
    return;
}

UINT16
EfiReadRegister16 (
    VOID *RegisterAddress
    )

/*++

Routine Description:

    This routine performs a 16-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

{

    EfiMemoryBarrier();
    return *((UINT16 *)RegisterAddress);
}

VOID
EfiWriteRegister16 (
    VOID *RegisterAddress,
    UINT16 Value
    )

/*++

Routine Description:

    This routine performs a 16-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    EfiMemoryBarrier();
    *((UINT16 *)RegisterAddress) = Value;
    EfiMemoryBarrier();
    return;
}

UINT8
EfiReadRegister8 (
    VOID *RegisterAddress
    )

/*++

Routine Description:

    This routine performs an 8-bit memory register read. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to read.

Return Value:

    Returns the value at the given register.

--*/

{

    EfiMemoryBarrier();
    return *((UINT8 *)RegisterAddress);
}

VOID
EfiWriteRegister8 (
    VOID *RegisterAddress,
    UINT8 Value
    )

/*++

Routine Description:

    This routine performs an 8-bit memory register write. The given address
    is assumed to be mapped with an uncached attribute.

Arguments:

    RegisterAddress - Supplies the virtual address of the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    EfiMemoryBarrier();
    *((UINT8 *)RegisterAddress) = Value;
    EfiMemoryBarrier();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


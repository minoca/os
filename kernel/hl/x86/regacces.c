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

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>

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

KERNEL_API
ULONG
HlReadRegister32 (
    PVOID RegisterAddress
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

    return *((volatile ULONG *)RegisterAddress);
}

KERNEL_API
VOID
HlWriteRegister32 (
    PVOID RegisterAddress,
    ULONG Value
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

    *((volatile ULONG *)RegisterAddress) = Value;
    return;
}

KERNEL_API
USHORT
HlReadRegister16 (
    PVOID RegisterAddress
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

    return *((volatile USHORT *)RegisterAddress);
}

KERNEL_API
VOID
HlWriteRegister16 (
    PVOID RegisterAddress,
    USHORT Value
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

    *((volatile USHORT *)RegisterAddress) = Value;
    return;
}

KERNEL_API
UCHAR
HlReadRegister8 (
    PVOID RegisterAddress
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

    return *((volatile UCHAR *)RegisterAddress);
}

KERNEL_API
VOID
HlWriteRegister8 (
    PVOID RegisterAddress,
    UCHAR Value
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

    *((volatile UCHAR *)RegisterAddress) = Value;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


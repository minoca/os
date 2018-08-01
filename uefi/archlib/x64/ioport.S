/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ioport.S

Abstract:

    This module implements assembly-based I/O port access routines for the
    AMD64 platform.

Author:

    Evan Green 11-Aug-2017

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/x64.inc>

//
// -------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ----------------------------------------------------------------------- Code
//

ASSEMBLY_FILE_HEADER

//
// UINT8
// EfiIoPortIn8 (
//     UINT16 Address
//     )
//

/*++

Routine Description:

    This routine performs an 8-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

FUNCTION(EfiIoPortIn8)
    movl    %edi, %edx              # Get port number in the right register.
    xorl    %eax, %eax              # Clear the high bits.
    inb     %dx, %al                # Perform the I/O port read.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(EfiIoPortIn8)

//
// UINT16
// EfiIoPortIn16 (
//     UINT16 Address
//     )
//

/*++

Routine Description:

    This routine performs a 16-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

FUNCTION(EfiIoPortIn16)
    movl    %edi, %edx              # Get port number in the right register.
    xorl    %eax, %eax              # Clear the high bits.
    inw     %dx, %ax                # Perform the I/O port read.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(EfiIoPortIn16)

//
// UINT32
// EfiIoPortIn32 (
//     UINT16 Address
//     )
//

/*++

Routine Description:

    This routine performs a 32-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

FUNCTION(EfiIoPortIn32)
    movl    %edi, %edx              # Get port number in the right register.
    xorl    %eax, %eax              # Clear the upper word.
    inl     %dx, %eax               # Perform the I/O port read.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(EfiIoPortIn32)

//
// VOID
// EfiIoPortOut8 (
//     UINT16 Address,
//     UINT8 Value
//     )
//

/*++

Routine Description:

    This routine performs an 8-bit write to the given I/O port.

Arguments:

    Address - Supplies the address to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION(EfiIoPortOut8)
    movl    %edi, %edx              # Get port number in the right register.
    movl    %esi, %eax              # Get the value in the right register.
    outb    %al, %dx                # Perform the I/O port write.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(EfiIoPortOut8)

//
// VOID
// EfiIoPortOut16 (
//     UINT16 Address,
//     UINT16 Value
//     )
//

/*++

Routine Description:

    This routine performs a 16-bit write to the given I/O port.

Arguments:

    Address - Supplies the address to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION(EfiIoPortOut16)
    movl    %edi, %edx              # Get port number in the right register.
    movl    %esi, %eax              # Get the value in the right register.
    outw    %ax, %dx                # Perform the I/O port write.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(EfiIoPortOut16)

//
// VOID
// EfiIoPortOut32 (
//     UINT16 Address,
//     UINT32 Value
//     )
//

/*++

Routine Description:

    This routine performs a 32-bit write to the given I/O port.

Arguments:

    Address - Supplies the address to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

FUNCTION(EfiIoPortOut32)
    movl    %edi, %edx              # Get port number in the right register.
    movl    %esi, %eax              # Get the value in the right register.
    outl    %eax, %dx               # Perform the I/O port write.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(EfiIoPortOut32)

//
// --------------------------------------------------------- Internal Functions
//


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
// HlIoPortInByte (
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

PROTECTED_FUNCTION(HlIoPortInByte)
    movl    %edi, %edx              # Get port number in the right register.
    xorl    %eax, %eax              # Clear the high bits.
    inb     %dx, %al                # Perform the I/O port read.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(HlIoPortInByte)

//
// UINT16
// HlIoPortInShort (
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

PROTECTED_FUNCTION(HlIoPortInShort)
    movl    %edi, %edx              # Get port number in the right register.
    xorl    %eax, %eax              # Clear the high bits.
    inw     %dx, %ax                # Perform the I/O port read.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(HlIoPortInShort)

//
// UINT32
// HlIoPortInLong (
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

PROTECTED_FUNCTION(HlIoPortInLong)
    movl    %edi, %edx              # Get port number in the right register.
    xorl    %eax, %eax              # Clear the upper word.
    inl     %dx, %eax               # Perform the I/O port read.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(HlIoPortInLong)

//
// VOID
// HlIoPortOutByte (
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

PROTECTED_FUNCTION(HlIoPortOutByte)
    movl    %edi, %edx              # Get port number in the right register.
    movl    %esi, %eax              # Get the value in the right register.
    outb    %al, %dx                # Perform the I/O port write.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(HlIoPortOutByte)

//
// VOID
// HlIoPortOutShort (
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

PROTECTED_FUNCTION(HlIoPortOutShort)
    movl    %edi, %edx              # Get port number in the right register.
    movl    %esi, %eax              # Get the value in the right register.
    outw    %ax, %dx                # Perform the I/O port write.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(HlIoPortOutShort)

//
// VOID
// HlIoPortOutLong (
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

PROTECTED_FUNCTION(HlIoPortOutLong)
    movl    %edi, %edx              # Get port number in the right register.
    movl    %esi, %eax              # Get the value in the right register.
    outl    %eax, %dx               # Perform the I/O port write.
    nop                             # Rest a touch.
    retq                            # Return.

END_FUNCTION(HlIoPortOutLong)

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ioport.S

Abstract:

    This module implements assembly-based I/O port access routines for the
    x86 platform.

Author:

    Evan Green 7-Aug-2013

Environment:

    Boot

--*/

##
## ------------------------------------------------------------------- Includes
##

#include <minoca/kernel/x86.inc>

##
## -------------------------------------------------------------------- Macros
##

##
## ---------------------------------------------------------------- Definitions
##

##
## -------------------------------------------------------------------- Globals
##

##
## ----------------------------------------------------------------------- Code
##

##
## .text specifies that this code belongs in the executable section.
##
## .code32 specifies that this is 32-bit protected mode code.
##

.text
.code32

##
## UINT8
## EfiIoPortIn8 (
##     UINT16 Address
##     )
##

/*++

Routine Description:

    This routine performs an 8-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

FUNCTION(EfiIoPortIn8)
    movl    4(%esp), %edx           # Get the I/O port.
    xorl    %eax, %eax              # Clear the high bits.
    inb     %dx, %al                # Perform the I/O port read.
    nop                             # Rest a touch.
    ret                             # Return.

END_FUNCTION(EfiIoPortIn8)

##
## UINT16
## EfiIoPortIn16 (
##     UINT16 Address
##     )
##

/*++

Routine Description:

    This routine performs a 16-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

FUNCTION(EfiIoPortIn16)
    movl    4(%esp), %edx           # Get the I/O port.
    xorl    %eax, %eax              # Clear the high bits.
    inw     %dx, %ax                # Perform the I/O port read.
    nop                             # Rest a touch.
    ret                             # Return.

END_FUNCTION(EfiIoPortIn16)

##
## UINT32
## EfiIoPortIn32 (
##     UINT16 Address
##     )
##

/*++

Routine Description:

    This routine performs a 32-bit read from the given I/O port.

Arguments:

    Address - Supplies the address to read from.

Return Value:

    Returns the value at that address.

--*/

FUNCTION(EfiIoPortIn32)
    movl    4(%esp), %edx           # Get the I/O port.
    inl     %dx, %eax               # Perform the I/O port read.
    nop                             # Rest a touch.
    ret                             # Return.

END_FUNCTION(EfiIoPortIn32)

##
## VOID
## EfiIoPortOut8 (
##     UINT16 Address,
##     UINT8 Value
##     )
##

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
    movl    4(%esp), %edx           # Get the I/O port.
    movl    8(%esp), %eax           # Get the value.
    outb    %al, %dx                # Perform the I/O port write.
    nop                             # Rest a touch.
    ret                             # Return.

END_FUNCTION(EfiIoPortOut8)

##
## VOID
## EfiIoPortOut16 (
##     UINT16 Address,
##     UINT16 Value
##     )
##

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
    movl    4(%esp), %edx           # Get the I/O port.
    movl    8(%esp), %eax           # Get the value.
    outw    %ax, %dx                # Perform the I/O port write.
    nop                             # Rest a touch.
    ret                             # Return.

END_FUNCTION(EfiIoPortOut16)

##
## VOID
## EfiIoPortOut32 (
##     UINT16 Address,
##     UINT32 Value
##     )
##

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
    movl    4(%esp), %edx           # Get the I/O port.
    movl    8(%esp), %eax           # Get the value.
    outl    %eax, %dx               # Perform the I/O port write.
    nop                             # Rest a touch.
    ret                             # Return.

END_FUNCTION(EfiIoPortOut32)

##
## --------------------------------------------------------- Internal Functions
##


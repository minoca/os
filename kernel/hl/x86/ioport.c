/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ioport.c

Abstract:

    This module implements legacy I/O port communication.

Author:

    Evan Green

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/ioport.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
UCHAR
HlIoPortInByte (
    USHORT InputPort
    )

/*++

Routine Description:

    This routine gets one byte from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a byte from.

Return Value:

    Returns the data at that port.

--*/

{

    UCHAR ReturnValue;

    //
    // Clear eax and mark it as destroyed.
    //

    asm volatile ("xorl %%eax,%%eax":::"eax");

    //
    // Execute the inb instruction. Register al has the output which should go
    // in ReturnValue, and dx has the input which should come from InputPort.
    //

    asm volatile ("inb %%dx,%%al":"=a" (ReturnValue):"d"(InputPort));
    return ReturnValue;
}

KERNEL_API
VOID
HlIoPortOutByte (
    USHORT OutputPort,
    BYTE OutputData
    )

/*++

Routine Description:

    This routine sends one byte to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a byte to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

{

    //
    // execute the outb instruction. There are no outputs, but both variables
    // are inputs to their respective registers.
    //

    asm volatile ("outb %%al,%%dx"::"a" (OutputData), "d"(OutputPort));

    //
    // Execute a no-op to give the bus time to settle.
    //

    asm volatile ("nop");
    return;
}

KERNEL_API
USHORT
HlIoPortInShort (
    USHORT InputPort
    )

/*++

Routine Description:

    This routine gets one 16 bit value from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a byte from.

Return Value:

    Returns the data at that port.

--*/

{

    USHORT ReturnValue;

    //
    // Clear eax and mark it as destroyed.
    //

    asm volatile ("xorl %%eax,%%eax":::"eax");

    //
    // Execute the inw instruction. Register ax has the output which should go
    // in ReturnValue, and dx has the input which should come from InputPort.
    //

    asm volatile ("inw %%dx,%%ax":"=a" (ReturnValue):"d"(InputPort));
    return ReturnValue;
}

KERNEL_API
VOID
HlIoPortOutShort (
    USHORT OutputPort,
    USHORT OutputData
    )

/*++

Routine Description:

    This routine sends one 16-bit value to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a byte to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

{

    //
    // execute the outb instruction. There are no outputs, but both variables
    // are inputs to their respective registers.
    //

    asm volatile ("outw %%ax,%%dx"::"a" (OutputData), "d"(OutputPort));

    //
    // Execute a no-op to give the bus time to settle.
    //

    asm volatile ("nop");
    return;
}

KERNEL_API
ULONG
HlIoPortInLong (
    USHORT InputPort
    )

/*++

Routine Description:

    This routine gets a 32-bit value from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a long from.

Return Value:

    Returns the data at that port.

--*/

{

    ULONG ReturnValue;

    //
    // Clear eax and mark it as destroyed.
    //

    asm volatile ("xorl %%eax,%%eax":::"eax");

    //
    // Execute the inl instruction. Register eax has the output which should go
    // in ReturnValue, and dx has the input which should come from InputPort.
    //

    asm volatile ("inl %%dx,%%eax":"=a" (ReturnValue):"d"(InputPort));
    return ReturnValue;
}

KERNEL_API
VOID
HlIoPortOutLong (
    USHORT OutputPort,
    ULONG OutputData
    )

/*++

Routine Description:

    This routine sends one 32-bit to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a long to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

{

    //
    // execute the outl instruction. There are no outputs, but both variables
    // are inputs to their respective registers.
    //

    asm volatile ("outl %%eax,%%dx"::"a" (OutputData), "d"(OutputPort));

    //
    // Execute a no-op to give the bus time to settle.
    //

    asm volatile ("nop");
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


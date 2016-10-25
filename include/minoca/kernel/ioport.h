/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ioport.h

Abstract:

    This header contains definitions for legacy I/O port control.

Author:

    Evan Green 3-Jul-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

KERNEL_API
UCHAR
HlIoPortInByte (
    USHORT InputPort
    );

/*++

Routine Description:

    This routine gets one byte from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a byte from.

Return Value:

    Returns the data at that port.

--*/

KERNEL_API
VOID
HlIoPortOutByte (
    USHORT OutputPort,
    BYTE OutputData
    );

/*++

Routine Description:

    This routine sends one byte to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a byte to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

KERNEL_API
USHORT
HlIoPortInShort (
    USHORT InputPort
    );

/*++

Routine Description:

    This routine gets one 16 bit value from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a byte from.

Return Value:

    Returns the data at that port.

--*/

KERNEL_API
VOID
HlIoPortOutShort (
    USHORT OutputPort,
    USHORT OutputData
    );

/*++

Routine Description:

    This routine sends one 16-bit value to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a byte to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

KERNEL_API
ULONG
HlIoPortInLong (
    USHORT InputPort
    );

/*++

Routine Description:

    This routine gets a 32-bit value from the specified legacy I/O port.

Arguments:

    InputPort - Supplies the port to receive a long from.

Return Value:

    Returns the data at that port.

--*/

KERNEL_API
VOID
HlIoPortOutLong (
    USHORT OutputPort,
    ULONG OutputData
    );

/*++

Routine Description:

    This routine sends one 32-bit to the specified legacy I/O port.

Arguments:

    OutputPort - Supplies the port to send a long to.

    OutputData - Supplies the data to send.

Return Value:

    None.

--*/

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

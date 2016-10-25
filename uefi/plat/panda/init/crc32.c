/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crc32.c

Abstract:

    This module implements support for calculating the CRC32 of a region of
    memory.

Author:

    Evan Green 3-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipInitializeCrc32Table (
    VOID
    );

UINT32
EfipCrc32ReverseBits (
    UINT32 Value
    );

//
// -------------------------------------------------------------------- Globals
//

UINT32 *EfiCrcTable = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipInitializeCrc32 (
    VOID *TableBuffer
    )

/*++

Routine Description:

    This routine initializes support for the early CRC32 support.

Arguments:

    TableBuffer - Supplies a pointer to a region of memory that will be used to
        store the CRC32 table.

Return Value:

    None.

--*/

{

    EfiCrcTable = TableBuffer;
    EfipInitializeCrc32Table();
    return;
}

UINT32
EfipInitCalculateCrc32 (
    VOID *Buffer,
    UINTN DataSize
    )

/*++

Routine Description:

    This routine computes the CRC of the given buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer to CRC.

    DataSize - Supplies the number of bytes to CRC.

Return Value:

    Returns the CRC of the buffer.

--*/

{

    UINT8 *Bytes;
    UINT32 Crc;
    UINTN Index;

    if (EfiCrcTable == NULL) {
        return 0;
    }

    Crc = 0xFFFFFFFF;
    Bytes = Buffer;
    for (Index = 0; Index < DataSize; Index += 1) {
        Crc = (Crc >> 8) ^ EfiCrcTable[(UINT8)Crc ^ *Bytes];
        Bytes += 1;
    }

    return Crc ^ 0xFFFFFFFF;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipInitializeCrc32Table (
    VOID
    )

/*++

Routine Description:

    This routine initializes the CRC table used to compute CRC32 sums.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINTN Index;
    UINTN TableEntry;
    UINT32 Value;

    for (TableEntry = 0; TableEntry < 0x100; TableEntry += 1) {
        Value = EfipCrc32ReverseBits((UINT32)TableEntry);
        for (Index = 0; Index < 8; Index += 1) {
            if ((Value & 0x80000000) != 0) {
                Value = (Value << 1) ^ 0x04C11DB7;

            } else {
                Value = Value << 1;
            }
        }

        EfiCrcTable[TableEntry] = EfipCrc32ReverseBits(Value);
    }

    return;
}

UINT32
EfipCrc32ReverseBits (
    UINT32 Value
    )

/*++

Routine Description:

    This routine reverses the bit order of a 32-bit value.

Arguments:

    Value - Supplies the value to reverse.

Return Value:

    Returns the mirror of the value.

--*/

{

    UINTN Index;
    UINT32 NewValue;

    NewValue = 0;
    for (Index = 0; Index < 32; Index += 1) {
        if ((Value & (1 << Index)) != 0) {
            NewValue |= 1 << (31 - Index);
        }
    }

    return NewValue;
}


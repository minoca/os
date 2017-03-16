/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crc32.c

Abstract:

    This module implements CRC-32 computation for the LZMA library.

Author:

    Evan Green 13-Mar-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/lzma.h>
#include "lzmap.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_CRC_POLYNOMIAL 0xEDB88320

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
// Create space for the CRC table. It will get filled in later since it's
// smaller to generate it than store it.
//

ULONG LzCrc32[0x100];

//
// ------------------------------------------------------------------ Functions
//

VOID
LzpCrcInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the CRC-32 table.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Bit;
    ULONG Index;
    ULONG Rotate;

    //
    // Don't reinitialize if it's already done.
    //

    if (LzCrc32[1] != 0) {
        return;
    }

    for (Index = 0; Index < 0x100; Index += 1) {
        Rotate = Index;
        for (Bit = 0; Bit < 8; Bit += 1) {
            Rotate = (Rotate >> 1) ^
                     (LZMA_CRC_POLYNOMIAL & ~((Rotate & 0x1) - 1));
        }

        LzCrc32[Index] = Rotate;
    }

    return;
}

ULONG
LzpComputeCrc32 (
    ULONG InitialCrc,
    PCVOID Buffer,
    ULONG Size
    )

/*++

Routine Description:

    This routine computes the CRC-32 on the given buffer of data.

Arguments:

    InitialCrc - Supplies an initial CRC value to start with. Supply 0
        initially.

    Buffer - Supplies a pointer to the buffer to compute the CRC32 of.

    Size - Supplies the size of the buffer, in bytes.

Return Value:

    Returns the CRC32 hash of the buffer.

--*/

{

    PCUCHAR Byte;
    ULONG Crc;
    PCUCHAR End;

    Byte = Buffer;
    End = Buffer + Size;
    Crc = InitialCrc ^ 0xFFFFFFFF;
    while (Byte != End) {
        Crc = LzCrc32[(Crc ^ *Byte) & 0xFF] ^ (Crc >> 8);
        Byte += 1;
    }

    Crc = Crc ^ 0xFFFFFFFF;
    return Crc;
}

//
// --------------------------------------------------------- Internal Functions
//


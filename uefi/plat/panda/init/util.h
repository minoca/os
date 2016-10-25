/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    util.h

Abstract:

    This header contains common utility definitions for the TI first stage
    loaders.

Author:

    Evan Green 19-Dec-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
EfipInitializeCrc32 (
    VOID *TableBuffer
    );

/*++

Routine Description:

    This routine initializes support for the early CRC32 support.

Arguments:

    TableBuffer - Supplies a pointer to a region of memory that will be used to
        store the CRC32 table.

Return Value:

    None.

--*/

UINT32
EfipInitCalculateCrc32 (
    VOID *Buffer,
    UINTN DataSize
    );

/*++

Routine Description:

    This routine computes the CRC of the given buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer to CRC.

    DataSize - Supplies the number of bytes to CRC.

Return Value:

    Returns the CRC of the buffer.

--*/

VOID
EfipInitZeroMemory (
    VOID *Buffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine zeroes memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to zero.

    Size - Supplies the number of bytes to zero.

Return Value:

    None.

--*/

VOID
EfipSerialPrintBuffer32 (
    CHAR8 *Title,
    VOID *Buffer,
    UINT32 Size
    );

/*++

Routine Description:

    This routine prints a buffer of 32-bit hex integers.

Arguments:

    Title - Supplies an optional pointer to a string to title the buffer.

    Buffer - Supplies the buffer to print.

    Size - Supplies the size of the buffer. This is assumed to be divisible by
        4.

Return Value:

    None.

--*/

VOID
EfipSerialPrintString (
    CHAR8 *String
    );

/*++

Routine Description:

    This routine prints a string to the serial console.

Arguments:

    String - Supplies a pointer to the string to send.

Return Value:

    None.

--*/

VOID
EfipSerialPrintHexInteger (
    UINT32 Value
    );

/*++

Routine Description:

    This routine prints a hex integer to the console.

Arguments:

    Value - Supplies the value to print.

Return Value:

    None.

--*/

VOID
EfipSerialPutCharacter (
    CHAR8 Character
    );

/*++

Routine Description:

    This routine prints a character to the serial console.

Arguments:

    Character - Supplies the character to send.

Return Value:

    None.

--*/


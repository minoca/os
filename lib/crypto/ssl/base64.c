/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    base64.c

Abstract:

    This module implements support for Base64 encoding and decoding.

Author:

    Evan Green 29-Jul-2015

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../cryptop.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CY_BASE64_ENCODE_STRING \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

UCHAR CyBase64DecodeTable[128] = {
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 62, 255, 255, 255, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255,
    255, 254, 255, 255, 255, 0, 1, 2, 3, 4, 5, 6,
    7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    19, 20, 21, 22, 23, 24, 25, 255, 255, 255, 255, 255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 255, 255, 255, 255, 255
};

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
UINTN
CyBase64GetDecodedLength (
    UINTN EncodedDataLength
    )

/*++

Routine Description:

    This routine returns the buffer size needed for a decode buffer with a
    given encoded buffer length. This may not be the actual decoded data size,
    but is a worst-case approximation.

Arguments:

    EncodedDataLength - Supplies the length of the encoded data, in bytes, not
        including a null terminator.

Return Value:

    Returns the appropriate size of the decoded data buffer.

--*/

{

    return ((EncodedDataLength + 3) / 4) * 3;
}

CRYPTO_API
UINTN
CyBase64GetEncodedLength (
    UINTN DataLength
    )

/*++

Routine Description:

    This routine returns the buffer size needed for a Base64 encoded buffer
    given a raw data buffer of the given size. This may not be the actual
    encoded data size, but is a worst-case approximation.

Arguments:

    DataLength - Supplies the length of the raw data to encode, in bytes.

Return Value:

    Returns the appropriate size of the encoded data buffer, including space
    for a null terminator.

--*/

{

    return (((DataLength + 2) / 3) * 4) + 1;
}

CRYPTO_API
BOOL
CyBase64Decode (
    PSTR EncodedData,
    UINTN EncodedDataLength,
    PUCHAR Data,
    PUINTN DataLength
    )

/*++

Routine Description:

    This routine decodes the given Base64 encoded data.

Arguments:

    EncodedData - Supplies a pointer to the encoded data string.

    EncodedDataLength - Supplies the length of the encoded data in bytes,
        not including a null terminator.

    Data - Supplies a pointer where the decoded data will be returned. It is
        assumed this buffer is big enough.

    DataLength - Supplies a pointer where the final length of the returned data
        will be returned.

Return Value:

    TRUE on success.

    FALSE if there was a data decoding error at the end.

--*/

{

    INT ByteIndex;
    ULONG ByteValue;
    INT Equals;
    UINTN InIndex;
    UINTN OutIndex;
    ULONG Value;

    ByteIndex = 0;
    Equals = 3;
    OutIndex = 0;
    Value = 0;
    for (InIndex = 0; InIndex < EncodedDataLength; InIndex += 1) {
        ByteValue = CyBase64DecodeTable[EncodedData[InIndex] & 0x7F];
        if (ByteValue == 255) {
            continue;
        }

        if (ByteValue == 254) {
            ByteValue = 0;
            Equals -= 1;
            if (Equals < 0) {
                return FALSE;
            }

        } else if (Equals != 3) {
            return FALSE;
        }

        Value = (Value << 6) | ByteValue;
        ByteIndex += 1;
        if (ByteIndex == 4) {
            Data[OutIndex] = (UCHAR)((Value >> 16) & 0xFF);
            OutIndex += 1;
            if (Equals > 1) {
                Data[OutIndex] = (UCHAR)((Value >> 8) & 0xFF);
                OutIndex += 1;
                if (Equals > 2) {
                    Data[OutIndex] = (UCHAR)(Value & 0xFF);
                    OutIndex += 1;
                }
            }

            ByteIndex = 0;
            Value = 0;
        }
    }

    *DataLength = OutIndex;
    return TRUE;
}

CRYPTO_API
VOID
CyBase64Encode (
    PUCHAR Data,
    UINTN DataLength,
    PSTR EncodedData,
    PUINTN EncodedDataLength
    )

/*++

Routine Description:

    This routine encodes the given data in Base64 format.

Arguments:

    Data - Supplies a pointer to the data to encode.

    DataLength - Supplies the length of the data to encode in bytes.

    EncodedData - Supplies a pointer where the encoded data will be returned.
        It is assumed this buffer is big enough to hold the encoded data.

    EncodedDataLength - Supplies a pointer where the actual length of the
        encoded data, including the null terminator, will be returned on
        success.

Return Value:

    None.

--*/

{

    INTN Index;
    UINTN OutIndex;
    PSTR Table;
    UINTN TableIndex;

    OutIndex = 0;
    Table = CY_BASE64_ENCODE_STRING;

    //
    // Loop converting units of 3 bytes into 4 characters.
    //

    for (Index = 0; Index < DataLength - 2; Index += 3) {
        TableIndex = (Data[Index] >> 2) & 0x3F;
        EncodedData[OutIndex] = Table[TableIndex];
        OutIndex += 1;
        TableIndex = ((Data[Index] & 0x03) << 4) |
                     ((Data[Index + 1] & 0xF0) >> 4);

        EncodedData[OutIndex] = Table[TableIndex];
        OutIndex += 1;
        TableIndex = ((Data[Index + 1] & 0x0F) << 2) |
                     ((Data[Index + 2] & 0xC0) >> 6);

        EncodedData[OutIndex] = Table[TableIndex];
        OutIndex += 1;
        TableIndex = Data[Index + 2] & 0x3F;
        EncodedData[OutIndex] = Table[TableIndex];
        OutIndex += 1;
    }

    //
    // Handle the last remaining couple of bytes. The first byte always spans
    // two characters. If the second byte exists, then there is one equals
    // at the end to round up to 4 characters. If the second byte doesn't exist,
    // then there are two equals.
    //

    if (Index < DataLength) {

        //
        // Add the first byte. If that's it, then add the last two bits of the
        // first byte plus two equals.
        //

        TableIndex = (Data[Index] >> 2) & 0x3F;
        EncodedData[OutIndex] = Table[TableIndex];
        OutIndex += 1;
        if (Index == DataLength - 1) {
            TableIndex = ((Data[Index] & 0x03) << 4);
            EncodedData[OutIndex] = Table[TableIndex];
            OutIndex += 1;
            EncodedData[OutIndex] = '=';
            OutIndex += 1;

        //
        // Add the next two bytes into three characters, plus an equals.
        //

        } else {
            TableIndex = ((Data[Index] & 0x03) << 4) |
                         ((Data[Index + 1] & 0xF0) >> 4);

            EncodedData[OutIndex] = Table[TableIndex];
            OutIndex += 1;
            TableIndex = (Data[Index + 1] & 0x0F) << 2;
            EncodedData[OutIndex] = Table[TableIndex];
            OutIndex += 1;
        }

        EncodedData[OutIndex] = '=';
        OutIndex += 1;
    }

    //
    // Add that null terminator.
    //

    EncodedData[OutIndex] = '\0';
    OutIndex += 1;
    *EncodedDataLength = OutIndex;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


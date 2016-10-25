/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    md5.c

Abstract:

    This module implements the MD5 hash routine.

Author:

    Evan Green 14-Jan-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "cryptop.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Define F, G, H, and I, the basic MD5 functions.
//

#define MD5_F(_ValueX, _ValueY, _ValueZ) \
    (((_ValueX) & (_ValueY)) | ((~_ValueX) & (_ValueZ)))

#define MD5_G(_ValueX, _ValueY, _ValueZ) \
    (((_ValueX) & (_ValueZ)) | ((_ValueY) & (~_ValueZ)))

#define MD5_H(_ValueX, _ValueY, _ValueZ) \
    ((_ValueX) ^ (_ValueY) ^ (_ValueZ))

#define MD5_I(_ValueX, _ValueY, _ValueZ) \
    ((_ValueY) ^ ((_ValueX) | (~_ValueZ)))

#define MD5_ROTATE_LEFT(_Value, _Count) \
    (((_Value) << (_Count)) | ((_Value) >> (32 - (_Count))))

//
// Define FF, GG, HH, and II transformations for rounds 1 through 4.
//

#define MD5_FF(_ValueA, _ValueB, _ValueC, _ValueD, _ValueX, _Shift, _ValueAc) \
    (_ValueA) += MD5_F((_ValueB), (_ValueC), (_ValueD)) + (_ValueX) + \
                 (ULONG)(_ValueAc); \
    (_ValueA) = MD5_ROTATE_LEFT((_ValueA), (_Shift)); \
    (_ValueA) += (_ValueB);

#define MD5_GG(_ValueA, _ValueB, _ValueC, _ValueD, _ValueX, _Shift, _ValueAc) \
    (_ValueA) += MD5_G((_ValueB), (_ValueC), (_ValueD)) + (_ValueX) + \
                 (ULONG)(_ValueAc); \
    (_ValueA) = MD5_ROTATE_LEFT((_ValueA), (_Shift)); \
    (_ValueA) += (_ValueB);

#define MD5_HH(_ValueA, _ValueB, _ValueC, _ValueD, _ValueX, _Shift, _ValueAc) \
    (_ValueA) += MD5_H((_ValueB), (_ValueC), (_ValueD)) + (_ValueX) + \
                 (ULONG)(_ValueAc); \
    (_ValueA) = MD5_ROTATE_LEFT((_ValueA), (_Shift)); \
    (_ValueA) += (_ValueB);

#define MD5_II(_ValueA, _ValueB, _ValueC, _ValueD, _ValueX, _Shift, _ValueAc) \
    (_ValueA) += MD5_I((_ValueB), (_ValueC), (_ValueD)) + (_ValueX) + \
                 (ULONG)(_ValueAc); \
    (_ValueA) = MD5_ROTATE_LEFT((_ValueA), (_Shift)); \
    (_ValueA) += (_ValueB);

//
// ---------------------------------------------------------------- Definitions
//

//
// Define constants for the MD5 transform routine.
//

#define MD5_S11 7
#define MD5_S12 12
#define MD5_S13 17
#define MD5_S14 22

#define MD5_S21 5
#define MD5_S22 9
#define MD5_S23 14
#define MD5_S24 20

#define MD5_S31 4
#define MD5_S32 11
#define MD5_S33 16
#define MD5_S34 23

#define MD5_S41 6
#define MD5_S42 10
#define MD5_S43 15
#define MD5_S44 21

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CypMd5ProcessMessage (
    PMD5_CONTEXT Context,
    UCHAR Block[MD5_BLOCK_SIZE]
    );

VOID
CypMd5PadMessage (
    PMD5_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
VOID
CyMd5Initialize (
    PMD5_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a MD5 context structure, preparing it to accept
    and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

{

    Context->Length = 0;
    Context->State[0] = 0x67452301;
    Context->State[1] = 0xEFCDAB89;
    Context->State[2] = 0x98BADCFE;
    Context->State[3] = 0x10325476;
    return;
}

CRYPTO_API
VOID
CyMd5AddContent (
    PMD5_CONTEXT Context,
    PVOID Message,
    UINTN Length
    )

/*++

Routine Description:

    This routine adds data to a MD5 digest.

Arguments:

    Context - Supplies a pointer to the initialized MD5 context.

    Message - Supplies a pointer to the buffer containing the bytes.

    Length - Supplies the length of the message buffer, in bytes.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;
    UINTN CurrentBlockBytes;
    UINTN Index;
    UINTN PartialSize;

    Bytes = Message;

    //
    // Figure out how many bytes are already in the block.
    //

    CurrentBlockBytes = (Context->Length / BITS_PER_BYTE) % MD5_BLOCK_SIZE;

    //
    // Update the length (in bits).
    //

    Context->Length += Length * BITS_PER_BYTE;
    PartialSize = MD5_BLOCK_SIZE - CurrentBlockBytes;
    if (Length >= PartialSize) {
        RtlCopyMemory(&(Context->MessageBlock[CurrentBlockBytes]),
                      Bytes,
                      PartialSize);

        CypMd5ProcessMessage(Context, Context->MessageBlock);

        //
        // Transform the other complete messages without copying.
        //

        for (Index = PartialSize;
             Index + MD5_BLOCK_SIZE - 1 < Length;
             Index += MD5_BLOCK_SIZE) {

            CypMd5ProcessMessage(Context, &(Bytes[Index]));
        }

        CurrentBlockBytes = 0;

    } else {
        Index = 0;
    }

    //
    // Copy the partial block to the working.
    //

    ASSERT(Length - Index + CurrentBlockBytes <= MD5_BLOCK_SIZE);

    if (Length - Index != 0) {
        RtlCopyMemory(&(Context->MessageBlock[CurrentBlockBytes]),
                      &(Bytes[Index]),
                      Length - Index);
    }

    return;
}

CRYPTO_API
VOID
CyMd5GetHash (
    PMD5_CONTEXT Context,
    UCHAR Hash[MD5_HASH_SIZE]
    )

/*++

Routine Description:

    This routine computes and returns the final MD5 hash value for the messages
    that have been previously entered.

Arguments:

    Context - Supplies a pointer to the initialized MD5 context.

    Hash - Supplies a pointer where the final hash value will be returned. This
        buffer must be MD5_HASH_SIZE length in bytes.

Return Value:

    None.

--*/

{

    CypMd5PadMessage(Context);
    RtlCopyMemory(Hash, Context->State, MD5_HASH_SIZE);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CypMd5ProcessMessage (
    PMD5_CONTEXT Context,
    UCHAR Block[MD5_BLOCK_SIZE]
    )

/*++

Routine Description:

    This routine processes the 512 bits in the current message block and adds
    them to the digest.

Arguments:

    Context - Supplies a pointer to the initialized MD5 context with a full
        message block.

    Block - Supplies a pointer to the block to digest.

Return Value:

    None.

--*/

{

    ULONG ValueA;
    ULONG ValueB;
    ULONG ValueC;
    ULONG ValueD;
    ULONG Working[MD5_HASH_SIZE];

    ASSERT(sizeof(Working) == MD5_BLOCK_SIZE);

    ValueA = Context->State[0];
    ValueB = Context->State[1];
    ValueC = Context->State[2];
    ValueD = Context->State[3];
    RtlCopyMemory(Working, Block, MD5_BLOCK_SIZE);

    //
    // Perform round 1.
    //

    MD5_FF(ValueA, ValueB, ValueC, ValueD, Working[0], MD5_S11, 0xD76AA478);
    MD5_FF(ValueD, ValueA, ValueB, ValueC, Working[1], MD5_S12, 0xE8C7B756);
    MD5_FF(ValueC, ValueD, ValueA, ValueB, Working[2], MD5_S13, 0x242070DB);
    MD5_FF(ValueB, ValueC, ValueD, ValueA, Working[3], MD5_S14, 0xC1BDCEEE);
    MD5_FF(ValueA, ValueB, ValueC, ValueD, Working[4], MD5_S11, 0xF57C0FAF);
    MD5_FF(ValueD, ValueA, ValueB, ValueC, Working[5], MD5_S12, 0x4787C62A);
    MD5_FF(ValueC, ValueD, ValueA, ValueB, Working[6], MD5_S13, 0xA8304613);
    MD5_FF(ValueB, ValueC, ValueD, ValueA, Working[7], MD5_S14, 0xFD469501);
    MD5_FF(ValueA, ValueB, ValueC, ValueD, Working[8], MD5_S11, 0x698098D8);
    MD5_FF(ValueD, ValueA, ValueB, ValueC, Working[9], MD5_S12, 0x8B44F7AF);
    MD5_FF(ValueC, ValueD, ValueA, ValueB, Working[10], MD5_S13, 0xFFFF5BB1);
    MD5_FF(ValueB, ValueC, ValueD, ValueA, Working[11], MD5_S14, 0x895CD7BE);
    MD5_FF(ValueA, ValueB, ValueC, ValueD, Working[12], MD5_S11, 0x6B901122);
    MD5_FF(ValueD, ValueA, ValueB, ValueC, Working[13], MD5_S12, 0xFD987193);
    MD5_FF(ValueC, ValueD, ValueA, ValueB, Working[14], MD5_S13, 0xA679438E);
    MD5_FF(ValueB, ValueC, ValueD, ValueA, Working[15], MD5_S14, 0x49B40821);

    //
    // Perform round 2.
    //

    MD5_GG(ValueA, ValueB, ValueC, ValueD, Working[1], MD5_S21, 0xF61E2562);
    MD5_GG(ValueD, ValueA, ValueB, ValueC, Working[6], MD5_S22, 0xC040B340);
    MD5_GG(ValueC, ValueD, ValueA, ValueB, Working[11], MD5_S23, 0x265E5A51);
    MD5_GG(ValueB, ValueC, ValueD, ValueA, Working[0], MD5_S24, 0xE9B6C7AA);
    MD5_GG(ValueA, ValueB, ValueC, ValueD, Working[5], MD5_S21, 0xD62F105D);
    MD5_GG(ValueD, ValueA, ValueB, ValueC, Working[10], MD5_S22, 0x02441453);
    MD5_GG(ValueC, ValueD, ValueA, ValueB, Working[15], MD5_S23, 0xD8A1E681);
    MD5_GG(ValueB, ValueC, ValueD, ValueA, Working[4], MD5_S24, 0xE7D3FBC8);
    MD5_GG(ValueA, ValueB, ValueC, ValueD, Working[9], MD5_S21, 0x21E1CDE6);
    MD5_GG(ValueD, ValueA, ValueB, ValueC, Working[14], MD5_S22, 0xC33707D6);
    MD5_GG(ValueC, ValueD, ValueA, ValueB, Working[3], MD5_S23, 0xF4D50D87);
    MD5_GG(ValueB, ValueC, ValueD, ValueA, Working[8], MD5_S24, 0x455A14ED);
    MD5_GG(ValueA, ValueB, ValueC, ValueD, Working[13], MD5_S21, 0xA9E3E905);
    MD5_GG(ValueD, ValueA, ValueB, ValueC, Working[2], MD5_S22, 0xFCEFA3F8);
    MD5_GG(ValueC, ValueD, ValueA, ValueB, Working[7], MD5_S23, 0x676F02D9);
    MD5_GG(ValueB, ValueC, ValueD, ValueA, Working[12], MD5_S24, 0x8D2A4C8A);

    //
    // Perform round 3.
    //

    MD5_HH(ValueA, ValueB, ValueC, ValueD, Working[5], MD5_S31, 0xFFFA3942);
    MD5_HH(ValueD, ValueA, ValueB, ValueC, Working[8], MD5_S32, 0x8771F681);
    MD5_HH(ValueC, ValueD, ValueA, ValueB, Working[11], MD5_S33, 0x6D9D6122);
    MD5_HH(ValueB, ValueC, ValueD, ValueA, Working[14], MD5_S34, 0xFDE5380C);
    MD5_HH(ValueA, ValueB, ValueC, ValueD, Working[1], MD5_S31, 0xA4BEEA44);
    MD5_HH(ValueD, ValueA, ValueB, ValueC, Working[4], MD5_S32, 0x4BDECFA9);
    MD5_HH(ValueC, ValueD, ValueA, ValueB, Working[7], MD5_S33, 0xF6BB4B60);
    MD5_HH(ValueB, ValueC, ValueD, ValueA, Working[10], MD5_S34, 0xBEBFBC70);
    MD5_HH(ValueA, ValueB, ValueC, ValueD, Working[13], MD5_S31, 0x289B7EC6);
    MD5_HH(ValueD, ValueA, ValueB, ValueC, Working[0], MD5_S32, 0xEAA127FA);
    MD5_HH(ValueC, ValueD, ValueA, ValueB, Working[3], MD5_S33, 0xD4EF3085);
    MD5_HH(ValueB, ValueC, ValueD, ValueA, Working[6], MD5_S34, 0x04881D05);
    MD5_HH(ValueA, ValueB, ValueC, ValueD, Working[9], MD5_S31, 0xD9D4D039);
    MD5_HH(ValueD, ValueA, ValueB, ValueC, Working[12], MD5_S32, 0xE6DB99E5);
    MD5_HH(ValueC, ValueD, ValueA, ValueB, Working[15], MD5_S33, 0x1FA27CF8);
    MD5_HH(ValueB, ValueC, ValueD, ValueA, Working[2], MD5_S34, 0xC4AC5665);

    //
    // Perform round 4.
    //

    MD5_II(ValueA, ValueB, ValueC, ValueD, Working[0], MD5_S41, 0xF4292244);
    MD5_II(ValueD, ValueA, ValueB, ValueC, Working[7], MD5_S42, 0x432AFF97);
    MD5_II(ValueC, ValueD, ValueA, ValueB, Working[14], MD5_S43, 0xAB9423A7);
    MD5_II(ValueB, ValueC, ValueD, ValueA, Working[5], MD5_S44, 0xFC93A039);
    MD5_II(ValueA, ValueB, ValueC, ValueD, Working[12], MD5_S41, 0x655B59C3);
    MD5_II(ValueD, ValueA, ValueB, ValueC, Working[3], MD5_S42, 0x8F0CCC92);
    MD5_II(ValueC, ValueD, ValueA, ValueB, Working[10], MD5_S43, 0xFFEFF47D);
    MD5_II(ValueB, ValueC, ValueD, ValueA, Working[1], MD5_S44, 0x85845DD1);
    MD5_II(ValueA, ValueB, ValueC, ValueD, Working[8], MD5_S41, 0x6FA87E4F);
    MD5_II(ValueD, ValueA, ValueB, ValueC, Working[15], MD5_S42, 0xFE2CE6E0);
    MD5_II(ValueC, ValueD, ValueA, ValueB, Working[6], MD5_S43, 0xA3014314);
    MD5_II(ValueB, ValueC, ValueD, ValueA, Working[13], MD5_S44, 0x4E0811A1);
    MD5_II(ValueA, ValueB, ValueC, ValueD, Working[4], MD5_S41, 0xF7537E82);
    MD5_II(ValueD, ValueA, ValueB, ValueC, Working[11], MD5_S42, 0xBD3AF235);
    MD5_II(ValueC, ValueD, ValueA, ValueB, Working[2], MD5_S43, 0x2AD7D2BB);
    MD5_II(ValueB, ValueC, ValueD, ValueA, Working[9], MD5_S44, 0xEB86D391);
    Context->State[0] += ValueA;
    Context->State[1] += ValueB;
    Context->State[2] += ValueC;
    Context->State[3] += ValueD;
    return;
}

VOID
CypMd5PadMessage (
    PMD5_CONTEXT Context
    )

/*++

Routine Description:

    This routine pads the message out to an even multiple of 512 bits.
    The standard specifies that the first padding bit must be a 1, and then the
    last 64 bits represent the length of the original message.

Arguments:

    Context - Supplies a pointer to the initialized MD5 context.

Return Value:

    None.

--*/

{

    UINTN Index;

    //
    // Check to see if the current message block is too small to hold the
    // initial padding bits and length. If so, process the block, then continue
    // padding onto a second block.
    //

    Index = (Context->Length / BITS_PER_BYTE) % MD5_BLOCK_SIZE;
    if (Index < 56) {
        Context->MessageBlock[Index] = 0x80;
        Index += 1;
        while (Index < 56) {
            Context->MessageBlock[Index] = 0;
            Index += 1;
        }

    } else {
        Context->MessageBlock[Index] = 0x80;
        Index += 1;
        while (Index < sizeof(Context->MessageBlock)) {
            Context->MessageBlock[Index] = 0;
            Index += 1;
        }

        CypMd5ProcessMessage(Context, Context->MessageBlock);
        RtlZeroMemory(Context->MessageBlock, 56);
    }

    //
    // Store the message length in the last 8 octets.
    //

    Context->MessageBlock[56] = (UCHAR)(Context->Length);
    Context->MessageBlock[57] = (UCHAR)(Context->Length >> 8);
    Context->MessageBlock[58] = (UCHAR)(Context->Length >> 16);
    Context->MessageBlock[59] = (UCHAR)(Context->Length >> 24);
    Context->MessageBlock[60] = (UCHAR)(Context->Length >> 32);
    Context->MessageBlock[61] = (UCHAR)(Context->Length >> (32 + 8));
    Context->MessageBlock[62] = (UCHAR)(Context->Length >> (32 + 16));
    Context->MessageBlock[63] = (UCHAR)(Context->Length >> (32 + 24));
    CypMd5ProcessMessage(Context, Context->MessageBlock);
    return;
}


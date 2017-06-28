/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sha1.c

Abstract:

    This module implements the SHA-1 hash function.

Author:

    Evan Green 13-Jan-2015

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "cryptop.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro rotates a 32-bit value left by the given number of bits.
//

#define SHA1_ROTATE32(_Value, _ShiftCount) \
    (((_Value) << (_ShiftCount)) | ((_Value) >> (32 - (_ShiftCount))))

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
CypSha1ProcessMessage (
    PSHA1_CONTEXT Context
    );

VOID
CypSha1PadMessage (
    PSHA1_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

const ULONG CySha1KConstants[4] = {
    0x5A827999UL,
    0x6ED9EBA1UL,
    0x8F1BBCDCUL,
    0xCA62C1D6UL
};

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
VOID
CySha1Initialize (
    PSHA1_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a SHA-1 context structure, preparing it to accept
    and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

{

    Context->Length = 0;
    Context->BlockIndex = 0;
    Context->IntermediateHash[0] = 0x67452301UL;
    Context->IntermediateHash[1] = 0xEFCDAB89UL;
    Context->IntermediateHash[2] = 0x98BADCFEUL;
    Context->IntermediateHash[3] = 0x10325476UL;
    Context->IntermediateHash[4] = 0xC3D2E1F0UL;
    return;
}

CRYPTO_API
VOID
CySha1AddContent (
    PSHA1_CONTEXT Context,
    PUCHAR Message,
    UINTN Length
    )

/*++

Routine Description:

    This routine adds data to a SHA-1 digest.

Arguments:

    Context - Supplies a pointer to the initialized SHA-1 context.

    Message - Supplies a pointer to the buffer containing the bytes.

    Length - Supplies the length of the message buffer, in bytes.

Return Value:

    None.

--*/

{

    while (Length != 0) {
        Context->MessageBlock[Context->BlockIndex] = *Message;
        Context->BlockIndex += 1;
        Context->Length += BITS_PER_BYTE;
        if (Context->BlockIndex == sizeof(Context->MessageBlock)) {
            CypSha1ProcessMessage(Context);
        }

        Message += 1;
        Length -= 1;
    }

    return;
}

CRYPTO_API
VOID
CySha1GetHash (
    PSHA1_CONTEXT Context,
    UCHAR Hash[SHA1_HASH_SIZE]
    )

/*++

Routine Description:

    This routine computes and returns the final SHA-1 hash value for the
    messages that have been previously entered.

Arguments:

    Context - Supplies a pointer to the initialized SHA-1 context.

    Hash - Supplies a pointer where the final hash value will be returned. This
        buffer must be SHA1_HASH_SIZE length in bytes.

Return Value:

    None.

--*/

{

    INT DigestIndex;
    INT ShiftAmount;

    CypSha1PadMessage(Context);
    Context->Length = 0;
    for (DigestIndex = 0; DigestIndex < SHA1_HASH_SIZE; DigestIndex += 1) {
        ShiftAmount = (sizeof(ULONG) - 1 - (DigestIndex % sizeof(ULONG))) *
                      BITS_PER_BYTE;

        Hash[DigestIndex] =
                      Context->IntermediateHash[DigestIndex / sizeof(ULONG)] >>
                      ShiftAmount;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CypSha1ProcessMessage (
    PSHA1_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes the 512 bits in the current message block and adds
    them to the digest.

Arguments:

    Context - Supplies a pointer to the initialized SHA-1 context with a full
        message block.

Return Value:

    None.

--*/

{

    ULONG Block[80];
    ULONG BlockA;
    ULONG BlockB;
    ULONG BlockC;
    ULONG BlockD;
    ULONG BlockE;
    INT Index;
    ULONG Value;

    //
    // Initialize the first 16 words in the block array.
    //

    for (Index = 0; Index < 16; Index += 1) {
        Block[Index] = ((ULONG)Context->MessageBlock[Index * 4] << 24) |
                       ((ULONG)Context->MessageBlock[(Index * 4) + 1] << 16) |
                       ((ULONG)Context->MessageBlock[(Index * 4) + 2] << 8) |
                       (Context->MessageBlock[(Index * 4) + 3]);
    }

    for (Index = 16; Index < 80; Index += 1) {
        Value = Block[Index - 3] ^ Block[Index - 8] ^ Block[Index - 14] ^
                Block[Index - 16];

        Block[Index] = SHA1_ROTATE32(Value, 1);
    }

    BlockA = Context->IntermediateHash[0];
    BlockB = Context->IntermediateHash[1];
    BlockC = Context->IntermediateHash[2];
    BlockD = Context->IntermediateHash[3];
    BlockE = Context->IntermediateHash[4];
    for (Index = 0; Index < 20; Index += 1) {
        Value = SHA1_ROTATE32(BlockA, 5) +
                ((BlockB & BlockC) | ((~BlockB) & BlockD)) +
                BlockE + Block[Index] + CySha1KConstants[0];

        BlockE = BlockD;
        BlockD = BlockC;
        BlockC = SHA1_ROTATE32(BlockB, 30);
        BlockB = BlockA;
        BlockA = Value;
    }

    for (Index = 20; Index < 40; Index += 1) {
        Value = SHA1_ROTATE32(BlockA, 5) + (BlockB ^ BlockC ^ BlockD) +
                BlockE + Block[Index] + CySha1KConstants[1];

        BlockE = BlockD;
        BlockD = BlockC;
        BlockC = SHA1_ROTATE32(BlockB, 30);
        BlockB = BlockA;
        BlockA = Value;
    }

    for (Index = 40; Index < 60; Index += 1) {
        Value = SHA1_ROTATE32(BlockA, 5) +
                ((BlockB & BlockC) | (BlockB & BlockD) | (BlockC & BlockD)) +
                BlockE + Block[Index] + CySha1KConstants[2];

        BlockE = BlockD;
        BlockD = BlockC;
        BlockC = SHA1_ROTATE32(BlockB, 30);
        BlockB = BlockA;
        BlockA = Value;
    }

    for (Index = 60; Index < 80; Index += 1) {
        Value = SHA1_ROTATE32(BlockA, 5) + (BlockB ^ BlockC ^ BlockD) +
                BlockE + Block[Index] + CySha1KConstants[3];

        BlockE = BlockD;
        BlockD = BlockC;
        BlockC = SHA1_ROTATE32(BlockB, 30);
        BlockB = BlockA;
        BlockA = Value;
    }

    Context->IntermediateHash[0] += BlockA;
    Context->IntermediateHash[1] += BlockB;
    Context->IntermediateHash[2] += BlockC;
    Context->IntermediateHash[3] += BlockD;
    Context->IntermediateHash[4] += BlockE;
    Context->BlockIndex = 0;
    return;
}

VOID
CypSha1PadMessage (
    PSHA1_CONTEXT Context
    )

/*++

Routine Description:

    This routine pads the message out to an even multiple of 512 bits.
    The standard specifies that the first padding bit must be a 1, and then the
    last 64 bits represent the length of the original message.

Arguments:

    Context - Supplies a pointer to the initialized SHA-1 context with a full
        message block.

Return Value:

    None.

--*/

{

    //
    // Check to see if the current message block is too small to hold the
    // initial padding bits and length. If so, process the block, then continue
    // padding onto a second block.
    //

    if (Context->BlockIndex > 55) {
        Context->MessageBlock[Context->BlockIndex] = 0x80;
        Context->BlockIndex += 1;
        while (Context->BlockIndex < sizeof(Context->MessageBlock)) {
            Context->MessageBlock[Context->BlockIndex] = 0;
            Context->BlockIndex += 1;
        }

        CypSha1ProcessMessage(Context);
        while (Context->BlockIndex < 56) {
            Context->MessageBlock[Context->BlockIndex] = 0;
            Context->BlockIndex += 1;
        }

    } else {
        Context->MessageBlock[Context->BlockIndex] = 0x80;
        Context->BlockIndex += 1;
        while (Context->BlockIndex < 56) {
            Context->MessageBlock[Context->BlockIndex] = 0;
            Context->BlockIndex += 1;
        }
    }

    //
    // Store the message length in the last 8 octets.
    //

    Context->MessageBlock[56] = (UCHAR)(Context->Length >> (32 + 24));
    Context->MessageBlock[57] = (UCHAR)(Context->Length >> (32 + 16));
    Context->MessageBlock[58] = (UCHAR)(Context->Length >> (32 + 8));
    Context->MessageBlock[59] = (UCHAR)(Context->Length >> 32);
    Context->MessageBlock[60] = (UCHAR)(Context->Length >> 24);
    Context->MessageBlock[61] = (UCHAR)(Context->Length >> 16);
    Context->MessageBlock[62] = (UCHAR)(Context->Length >> 8);
    Context->MessageBlock[63] = (UCHAR)(Context->Length);
    CypSha1ProcessMessage(Context);
    return;
}


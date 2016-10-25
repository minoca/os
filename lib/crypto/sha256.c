/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sha256.c

Abstract:

    This module implements the SHA-256 hashing function.

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

#define SHA256_ROTATE_LEFT(_Value, _Count) \
    (((_Value) << (_Count)) | ((_Value) >> (32 - (_Count))))

#define SHA256_ROTATE_RIGHT(_Value, _Count) \
    (((_Value) >> (_Count)) | ((_Value) << (32 - (_Count))))

#define SHA256_CH(_ValueX, _ValueY, _ValueZ) \
    (((_ValueX) & (_ValueY)) ^ (~(_ValueX) & (_ValueZ)))

#define SHA256_MAJ(_ValueX, _ValueY, _ValueZ)               \
    (((_ValueX) & (_ValueY)) ^ ((_ValueX) & (_ValueZ)) ^    \
     ((_ValueY) & (_ValueZ)))

#define SHA256_EP0(_Value)              \
    (SHA256_ROTATE_RIGHT(_Value, 2) ^   \
     SHA256_ROTATE_RIGHT(_Value, 13) ^  \
     SHA256_ROTATE_RIGHT(_Value, 22))

#define SHA256_EP1(_Value)              \
    (SHA256_ROTATE_RIGHT(_Value, 6) ^   \
     SHA256_ROTATE_RIGHT(_Value, 11) ^  \
     SHA256_ROTATE_RIGHT(_Value, 25))

#define SHA256_SIG0(_Value)             \
    (SHA256_ROTATE_RIGHT(_Value, 7) ^   \
     SHA256_ROTATE_RIGHT(_Value, 18) ^  \
     ((_Value) >> 3))

#define SHA256_SIG1(_Value)             \
    (SHA256_ROTATE_RIGHT(_Value, 17) ^   \
     SHA256_ROTATE_RIGHT(_Value, 19) ^  \
     ((_Value) >> 10))

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
CypSha256ProcessMessage (
    PSHA256_CONTEXT Context
    );

VOID
CypSha256PadMessage (
    PSHA256_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

const ULONG CySha256KConstants[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1,
    0x923F82A4, 0xAB1C5ED5, 0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174, 0xE49B69C1, 0xEFBE4786,
    0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147,
    0x06CA6351, 0x14292967, 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85, 0xA2BFE8A1, 0xA81A664B,
    0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A,
    0x5B9CCA4F, 0x682E6FF3, 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2
};

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
VOID
CySha256Initialize (
    PSHA256_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a SHA-256 context structure, preparing it to
    accept and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

{

    Context->Length = 0;
    Context->BlockIndex = 0;
    Context->IntermediateHash[0] = 0x6A09E667;
    Context->IntermediateHash[1] = 0xBB67AE85;
    Context->IntermediateHash[2] = 0x3C6EF372;
    Context->IntermediateHash[3] = 0xA54FF53A;
    Context->IntermediateHash[4] = 0x510E527F;
    Context->IntermediateHash[5] = 0x9B05688C;
    Context->IntermediateHash[6] = 0x1F83D9AB;
    Context->IntermediateHash[7] = 0x5BE0CD19;
    return;
}

CRYPTO_API
VOID
CySha256AddContent (
    PSHA256_CONTEXT Context,
    PVOID Message,
    UINTN Length
    )

/*++

Routine Description:

    This routine adds data to a SHA-256 digest.

Arguments:

    Context - Supplies a pointer to the initialized SHA-256 context.

    Message - Supplies a pointer to the buffer containing the bytes.

    Length - Supplies the length of the message buffer, in bytes.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;

    Bytes = Message;
    while (Length != 0) {
        Context->MessageBlock[Context->BlockIndex] = *Bytes;
        Context->BlockIndex += 1;
        if (Context->BlockIndex == sizeof(Context->MessageBlock)) {
            CypSha256ProcessMessage(Context);
            Context->Length += sizeof(Context->MessageBlock) * BITS_PER_BYTE;
            Context->BlockIndex = 0;
        }

        Bytes += 1;
        Length -= 1;
    }

    return;
}

CRYPTO_API
VOID
CySha256GetHash (
    PSHA256_CONTEXT Context,
    UCHAR Hash[SHA256_HASH_SIZE]
    )

/*++

Routine Description:

    This routine computes and returns the final SHA-256 hash value for the
    messages that have been previously entered.

Arguments:

    Context - Supplies a pointer to the initialized SHA-256 context.

    Hash - Supplies a pointer where the final hash value will be returned. This
        buffer must be SHA256_HASH_SIZE length in bytes.

Return Value:

    None.

--*/

{

    INT DigestIndex;
    INT ShiftAmount;

    CypSha256PadMessage(Context);
    Context->Length = 0;

    //
    // Copy the key and convert to big endian at the same time.
    //

    for (DigestIndex = 0; DigestIndex < SHA256_HASH_SIZE; DigestIndex += 1) {
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
CypSha256ProcessMessage (
    PSHA256_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes the 512 bits in the current message block and adds
    them to the digest.

Arguments:

    Context - Supplies a pointer to the initialized SHA-256 context with a full
        message block.

Return Value:

    None.

--*/

{

    ULONG Block[64];
    ULONG BlockA;
    ULONG BlockB;
    ULONG BlockC;
    ULONG BlockD;
    ULONG BlockE;
    ULONG BlockF;
    ULONG BlockG;
    ULONG BlockH;
    UINTN BlockIndex;
    UINTN ByteIndex;
    ULONG Working1;
    ULONG Working2;

    ByteIndex = 0;
    for (BlockIndex = 0; BlockIndex < 16; BlockIndex += 1) {
        Block[BlockIndex] = (Context->MessageBlock[ByteIndex] << 24) |
                            (Context->MessageBlock[ByteIndex + 1] << 16) |
                            (Context->MessageBlock[ByteIndex + 2] << 8) |
                            Context->MessageBlock[ByteIndex + 3];

        ByteIndex += 4;
    }

    while (BlockIndex < 64) {
        Block[BlockIndex] = SHA256_SIG1(Block[BlockIndex - 2]) +
                            Block[BlockIndex - 7] +
                            SHA256_SIG0(Block[BlockIndex - 15]) +
                            Block[BlockIndex - 16];

        BlockIndex += 1;
    }

    BlockA = Context->IntermediateHash[0];
    BlockB = Context->IntermediateHash[1];
    BlockC = Context->IntermediateHash[2];
    BlockD = Context->IntermediateHash[3];
    BlockE = Context->IntermediateHash[4];
    BlockF = Context->IntermediateHash[5];
    BlockG = Context->IntermediateHash[6];
    BlockH = Context->IntermediateHash[7];
    for (BlockIndex = 0; BlockIndex < 64; BlockIndex += 1) {
        Working1 = BlockH +
                   SHA256_EP1(BlockE) +
                   SHA256_CH(BlockE, BlockF, BlockG) +
                   CySha256KConstants[BlockIndex] +
                   Block[BlockIndex];

        Working2 = SHA256_EP0(BlockA) + SHA256_MAJ(BlockA, BlockB, BlockC);
        BlockH = BlockG;
        BlockG = BlockF;
        BlockF = BlockE;
        BlockE = BlockD + Working1;
        BlockD = BlockC;
        BlockC = BlockB;
        BlockB = BlockA;
        BlockA = Working1 + Working2;
    }

    Context->IntermediateHash[0] += BlockA;
    Context->IntermediateHash[1] += BlockB;
    Context->IntermediateHash[2] += BlockC;
    Context->IntermediateHash[3] += BlockD;
    Context->IntermediateHash[4] += BlockE;
    Context->IntermediateHash[5] += BlockF;
    Context->IntermediateHash[6] += BlockG;
    Context->IntermediateHash[7] += BlockH;
    return;
}

VOID
CypSha256PadMessage (
    PSHA256_CONTEXT Context
    )

/*++

Routine Description:

    This routine pads the message out to an even multiple of 512 bits.
    The standard specifies that the first padding bit must be a 1, and then the
    last 64 bits represent the length of the original message.

Arguments:

    Context - Supplies a pointer to the initialized SHA-256 context with a full
        message block.

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

    Index = Context->BlockIndex;
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

        CypSha256ProcessMessage(Context);
        RtlZeroMemory(Context->MessageBlock, 56);
    }

    Context->Length += Context->BlockIndex * BITS_PER_BYTE;

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
    CypSha256ProcessMessage(Context);
    return;
}


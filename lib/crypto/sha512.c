/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sha512.c

Abstract:

    This module implements support for the SHA-512 hashing function.

Author:

    Evan Green 6-Mar-2015

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
// This macro adds a 64 bit value to a 128 bit value (stored as an array of 2
// 64 bit values).
//

#define SHA512_ADD128(_Result128, _Add64)           \
    {                                               \
        (_Result128)[0] += (ULONGLONG)(_Add64);     \
        if ((_Result128)[0] < (_Add64)) {           \
            (_Result128)[1] += 1;                   \
        }                                           \
    }

//
// This macro shifts the given value right.
//

#define SHA512_R(_Shift, _Value) ((_Value) >> (_Shift))

//
// This macro performs a 64-bit rotate right operation.
//

#define SHA512_S(_Amount, _Value) \
    (((_Value) >> (_Amount)) | ((_Value) << (64 - (_Amount))))

//
// These macros perform the logical operations used by SHA-512.
//

#define SHA512_CH(_BlockX, _BlockY, _BlockZ)                \
    (((_BlockX) & (_BlockY)) ^ ((~(_BlockX)) & (_BlockZ)))

#define SHA512_MAJ(_BlockX, _BlockY, _BlockZ)               \
    (((_BlockX) & (_BlockY)) ^ ((_BlockX) & (_BlockZ)) ^    \
     ((_BlockY) & (_BlockZ)))

#define SHA512_SIGMA0_HIGH(_Value) \
    (SHA512_S(28, (_Value)) ^ SHA512_S(34, (_Value)) ^ SHA512_S(39, (_Value)))

#define SHA512_SIGMA1_HIGH(_Value) \
    (SHA512_S(14, (_Value)) ^ SHA512_S(18, (_Value)) ^ SHA512_S(41, (_Value)))

#define SHA512_SIGMA0_LOW(_Value) \
    (SHA512_S(1, (_Value)) ^ SHA512_S(8, (_Value)) ^ SHA512_R(7, (_Value)))

#define SHA512_SIGMA1_LOW(_Value) \
    (SHA512_S(19, (_Value)) ^ SHA512_S(61, (_Value)) ^ SHA512_R(6, (_Value)))

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
CypSha512PadMessage (
    PSHA512_CONTEXT Context
    );

VOID
CypSha512ProcessMessage (
    PSHA512_CONTEXT Context,
    PVOID Block
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the SHA-512 K constants.
//

const ULONGLONG CySha512KConstants[80] = {
    0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL,
    0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
    0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL,
    0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
    0xD807AA98A3030242ULL, 0x12835B0145706FBEULL,
    0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
    0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL,
    0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
    0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL,
    0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
    0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL,
    0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
    0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL,
    0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
    0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL,
    0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
    0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL,
    0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
    0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL,
    0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
    0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL,
    0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
    0xD192E819D6EF5218ULL, 0xD69906245565A910ULL,
    0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
    0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL,
    0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
    0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL,
    0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
    0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL,
    0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
    0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL,
    0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
    0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL,
    0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
    0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL,
    0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
    0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL,
    0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
    0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL,
    0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL
};

//
// Define the SHA-512 initial values.
//

const ULONGLONG CySha512InitialState[8] = {
    0x6A09E667F3BCC908ULL,
    0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL,
    0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL,
    0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL,
    0x5BE0CD19137E2179ULL
};

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
VOID
CySha512Initialize (
    PSHA512_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes a SHA-512 context structure, preparing it to
    accept and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

{

    RtlZeroMemory(Context->MessageBlock, SHA512_BLOCK_SIZE);
    RtlCopyMemory(Context->IntermediateHash,
                  (PVOID)CySha512InitialState,
                  SHA512_HASH_SIZE);

    Context->Length[0] = 0;
    Context->Length[1] = 0;
    return;
}

CRYPTO_API
VOID
CySha512AddContent (
    PSHA512_CONTEXT Context,
    PVOID Message,
    UINTN Length
    )

/*++

Routine Description:

    This routine adds data to a SHA-512 digest.

Arguments:

    Context - Supplies a pointer to the initialized SHA-512 context.

    Message - Supplies a pointer to the buffer containing the bytes.

    Length - Supplies the length of the message buffer, in bytes.

Return Value:

    None.

--*/

{

    PUCHAR Bytes;
    UINTN FreeSpace;
    UINTN UsedSpace;

    if (Length == 0) {
        return;
    }

    Bytes = Message;

    //
    // Handle the awkward partial block at the beginning.
    //

    UsedSpace = (Context->Length[0] >> 3) % SHA512_BLOCK_SIZE;
    if (UsedSpace > 0) {
        FreeSpace = SHA512_BLOCK_SIZE - UsedSpace;

        //
        // If the incoming data fills the buffer, copy what is possible and
        // process the block.
        //

        if (Length >= FreeSpace) {
            RtlCopyMemory(&(Context->MessageBlock[UsedSpace]),
                          Bytes,
                          FreeSpace);

            SHA512_ADD128(Context->Length, FreeSpace << 3);
            Length -= FreeSpace;
            Bytes += FreeSpace;
            CypSha512ProcessMessage(Context, Context->MessageBlock);

        //
        // Easy street, this buffer's not full yet.
        //

        } else {
            RtlCopyMemory(&(Context->MessageBlock[UsedSpace]), Bytes, Length);
            SHA512_ADD128(Context->Length, Length << 3);
            return;
        }
    }

    //
    // Add whole blocks.
    //

    while (Length >= SHA512_BLOCK_SIZE) {
        CypSha512ProcessMessage(Context, Bytes);
        SHA512_ADD128(Context->Length, SHA512_BLOCK_SIZE << 3);
        Length -= SHA512_BLOCK_SIZE;
        Bytes += SHA512_BLOCK_SIZE;
    }

    //
    // Add any remainder to the current message buffer.
    //

    if (Length > 0) {
        RtlCopyMemory(Context->MessageBlock, Bytes, Length);
        SHA512_ADD128(Context->Length, Length << 3);
    }

    return;
}

CRYPTO_API
VOID
CySha512GetHash (
    PSHA512_CONTEXT Context,
    UCHAR Hash[SHA512_HASH_SIZE]
    )

/*++

Routine Description:

    This routine computes and returns the final SHA-512 hash value for the
    messages that have been previously entered.

Arguments:

    Context - Supplies a pointer to the initialized SHA-512 context.

    Hash - Supplies a pointer where the final hash value will be returned. This
        buffer must be SHA512_HASH_SIZE length in bytes.

Return Value:

    None.

--*/

{

    PULONGLONG HashWords64;
    UINTN Index;

    HashWords64 = (PULONGLONG)Hash;
    if (HashWords64 != NULL) {
        CypSha512PadMessage(Context);

        //
        // Swizzle the bytes to little endian.
        //

        for (Index = 0;
             Index < (SHA512_HASH_SIZE / sizeof(ULONGLONG));
             Index += 1) {

            *HashWords64 =
                        RtlByteSwapUlonglong(Context->IntermediateHash[Index]);

            HashWords64 += 1;
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CypSha512PadMessage (
    PSHA512_CONTEXT Context
    )

/*++

Routine Description:

    This routine pads the message out to an even multiple of 512 bits.
    The standard specifies that the first padding bit must be a 1, and then the
    last 64 bits represent the length of the original message.

Arguments:

    Context - Supplies a pointer to the initialized SHA-512 context to be
        finalized.

Return Value:

    None.

--*/

{

    PULONGLONG Pointer64;
    UINTN UsedSpace;

    UsedSpace = (Context->Length[0] >> 3) % SHA512_BLOCK_SIZE;

    //
    // Flip the size to big-endian.
    //

    Context->Length[0] = RtlByteSwapUlonglong(Context->Length[0]);
    Context->Length[1] = RtlByteSwapUlonglong(Context->Length[1]);
    if (UsedSpace > 0) {
        Context->MessageBlock[UsedSpace] = 0x80;
        UsedSpace += 1;
        if (UsedSpace <= SHA512_SHORT_BLOCK_SIZE) {
            RtlZeroMemory(&(Context->MessageBlock[UsedSpace]),
                          SHA512_SHORT_BLOCK_SIZE - UsedSpace);

        } else {
            if (UsedSpace < SHA512_BLOCK_SIZE) {
                RtlZeroMemory(&(Context->MessageBlock[UsedSpace]),
                              SHA512_BLOCK_SIZE - UsedSpace);
            }

            CypSha512ProcessMessage(Context, Context->MessageBlock);

            //
            // Prepare for the final transform.
            //

            RtlZeroMemory(Context->MessageBlock, SHA512_SHORT_BLOCK_SIZE);
        }

    } else {

        //
        // Prepare for the final transform.
        //

        RtlZeroMemory(Context->MessageBlock, SHA512_SHORT_BLOCK_SIZE);
        Context->MessageBlock[0] = 0x80;
    }

    Pointer64 = (PULONGLONG)(Context->MessageBlock + SHA512_SHORT_BLOCK_SIZE);
    Pointer64[0] = Context->Length[1];
    Pointer64[1] = Context->Length[0];
    CypSha512ProcessMessage(Context, Context->MessageBlock);
    return;
}

VOID
CypSha512ProcessMessage (
    PSHA512_CONTEXT Context,
    PVOID Block
    )

/*++

Routine Description:

    This routine performs the actual SHA-512 hash transformation on a complete
    message block.

Arguments:

    Context - Supplies a pointer to the initialized SHA-512 context.

    Block - Supplies a pointer to the block data.

Return Value:

    None.

--*/

{

    ULONGLONG BlockA;
    ULONGLONG BlockB;
    ULONGLONG BlockC;
    ULONGLONG BlockD;
    ULONGLONG BlockE;
    ULONGLONG BlockF;
    ULONGLONG BlockG;
    ULONGLONG BlockH;
    ULONGLONG BlockS0;
    ULONGLONG BlockS1;
    ULONGLONG BlockT1;
    ULONGLONG BlockT2;
    PULONGLONG Buffer;
    PULONGLONG Data;
    UINTN Iteration;

    Buffer = (PULONGLONG)(Context->MessageBlock);
    Data = Block;
    BlockA = Context->IntermediateHash[0];
    BlockB = Context->IntermediateHash[1];
    BlockC = Context->IntermediateHash[2];
    BlockD = Context->IntermediateHash[3];
    BlockE = Context->IntermediateHash[4];
    BlockF = Context->IntermediateHash[5];
    BlockG = Context->IntermediateHash[6];
    BlockH = Context->IntermediateHash[7];
    for (Iteration = 0; Iteration < 16; Iteration += 1) {
        Buffer[Iteration] = RtlByteSwapUlonglong(*Data);
        Data += 1;

        //
        // Apply the compression function to update blocks A through H.
        //

        BlockT1 = BlockH +
                  SHA512_SIGMA1_HIGH(BlockE) +
                  SHA512_CH(BlockE, BlockF, BlockG) +
                  CySha512KConstants[Iteration] +
                  Buffer[Iteration];

        BlockT2 = SHA512_SIGMA0_HIGH(BlockA) +
                  SHA512_MAJ(BlockA, BlockB, BlockC);

        BlockH = BlockG;
        BlockG = BlockF;
        BlockF = BlockE;
        BlockE = BlockD + BlockT1;
        BlockD = BlockC;
        BlockC = BlockB;
        BlockB = BlockA;
        BlockA = BlockT1 + BlockT2;
    }

    //
    // Do the remainder of the loops now that the reversals are out of the way.
    //

    while (Iteration < 80) {
        BlockS0 = Buffer[(Iteration + 1) & 0x0F];
        BlockS0 = SHA512_SIGMA0_LOW(BlockS0);
        BlockS1 = Buffer[(Iteration + 14) & 0x0F];
        BlockS1 = SHA512_SIGMA1_LOW(BlockS1);
        Buffer[Iteration & 0x0F] += BlockS1 + Buffer[(Iteration + 9) & 0x0F] +
                                    BlockS0;

        BlockT1 = BlockH +
                  SHA512_SIGMA1_HIGH(BlockE) +
                  SHA512_CH(BlockE, BlockF, BlockG) +
                  CySha512KConstants[Iteration] +
                  Buffer[Iteration & 0x0F];

        BlockT2 = SHA512_SIGMA0_HIGH(BlockA) +
                  SHA512_MAJ(BlockA, BlockB, BlockC);

        BlockH = BlockG;
        BlockG = BlockF;
        BlockF = BlockE;
        BlockE = BlockD + BlockT1;
        BlockD = BlockC;
        BlockC = BlockB;
        BlockB = BlockA;
        BlockA = BlockT1 + BlockT2;
        Iteration += 1;
    }

    //
    // Put the values back into the intermediate hash context.
    //

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


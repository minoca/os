/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    aes.c

Abstract:

    This module implements the AES encryption and decryption routines.

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
// This macro performs a simple byte swap on a 32 bit value.
//

#define AES_BYTE_SWAP32(_Value)               \
    ((((ULONG)(_Value) & 0xFF000000) >> 24) | \
     (((ULONG)(_Value) & 0x00FF0000) >> 8) |  \
     (((ULONG)(_Value) & 0x0000FF00) << 8) |  \
     (((ULONG)(_Value) & 0x000000FF) << 24))

//
// Define rotation methods that rotate a 32 bit value by either 1 byte, 2 bytes,
// or 3 bytes.
//

#define AES_ROTATE1(_Value) (((_Value) << 24) | ((_Value) >> 8))
#define AES_ROTATE2(_Value) (((_Value) << 16) | ((_Value) >> 16))
#define AES_ROTATE3(_Value) (((_Value) << 8) | ((_Value) >> 24))

//
// This macro does 4 multiplies by 2 in a finite field.
//

#define AES_FINITE_MULTIPLY_2(_Value, _TemporaryVariable)   \
    ((_TemporaryVariable) = ((_Value) & 0x80808080),        \
    ((((_Value) + (_Value)) & 0xFEFEFEFE) ^                 \
     (((_TemporaryVariable) - ((_TemporaryVariable) >> 7)) & 0x1B1B1B1B)))

//
// This macro does the inverse mix columns operation.
//

#define AES_INVERSE_MIX_COLUMNS(_Value, _F2, _F4, _F8, _F9) \
    ((_F2) = AES_FINITE_MULTIPLY_2(_Value, _F2),            \
     (_F4) = AES_FINITE_MULTIPLY_2(_F2, _F4),               \
     (_F8) = AES_FINITE_MULTIPLY_2(_F4, _F8),               \
     (_F9) = (_Value) ^ (_F8),                              \
     (_F8) = ((_F2) ^ (_F4) ^ (_F8)),                       \
     (_F2) ^= (_F9),                                        \
     (_F4) ^= (_F9),                                        \
     (_F8) ^= AES_ROTATE3(_F2),                             \
     (_F8) ^= AES_ROTATE2(_F4),                             \
     (_F8) ^ AES_ROTATE1(_F9))

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
CypAesEncryptBlock (
    PAES_CONTEXT Context,
    PULONG Block
    );

VOID
CypAesDecryptBlock (
    PAES_CONTEXT Context,
    PULONG Block
    );

UCHAR
CypAesXtime (
    UCHAR Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the S-Box and Inverse S-Box values.
//

static const UCHAR CyAesSbox[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5,
    0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC,
    0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B,
    0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85,
    0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17,
    0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88,
    0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9,
    0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6,
    0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94,
    0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68,
    0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16,
};

static const UCHAR CyAesInvertedSbox[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38,
    0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,
    0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D,
    0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2,
    0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA,
    0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A,
    0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,
    0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA,
    0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85,
    0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20,
    0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31,
    0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,
    0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0,
    0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26,
    0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

static const UCHAR CyAesRcon[30]= {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x1B, 0x36, 0x6C, 0xD8, 0xAB, 0x4D, 0x9A, 0x2F,
    0x5E, 0xBC, 0x63, 0xC6, 0x97, 0x35, 0x6A, 0xD4,
    0xB3, 0x7D, 0xFA, 0xEF, 0xC5, 0x91,
};

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
VOID
CyAesInitialize (
    PAES_CONTEXT Context,
    AES_CIPHER_MODE Mode,
    PUCHAR Key,
    PUCHAR InitializationVector
    )

/*++

Routine Description:

    This routine initializes an AES context structure, making it ready to
    encrypt and decrypt data.

Arguments:

    Context - Supplies a pointer to the AES state.

    Mode - Supplies the mode of AES to use.

    Key - Supplies the encryption/decryption key to use.

    InitializationVector - Supplies the initialization vector to start with.
        This doubles as the initial counter value for counter mode, which
        should be provided in big-endian byte order.

Return Value:

    None. The AES context will be initialized and ready for operation.

--*/

{

    PUCHAR CurrentRcon;
    INT ExpandedKeyRange;
    INT Index;
    ULONG KeyValue;
    PULONG LongPointer;
    INT Words;
    ULONG WorkingKey;

    switch (Mode) {
    case AesModeCbc128:
    case AesModeEcb128:
    case AesModeCtr128:
        Context->Rounds = 10;
        Context->KeySize = AES_CBC128_KEY_SIZE;
        break;

    case AesModeCbc256:
    case AesModeEcb256:
    case AesModeCtr256:
        Context->Rounds = 14;
        Context->KeySize = AES_CBC256_KEY_SIZE;
        break;

    default:
        return;
    }

    //
    // Copy the initial key, performing byte swapping.
    //

    Words = Context->KeySize / sizeof(ULONG);
    LongPointer = (PULONG)(&(Context->Keys[0]));
    for (Index = 0; Index < Words; Index += 1) {
        LongPointer[Index] = ((ULONG)(Key[0]) << 24) |
                             ((ULONG)(Key[1]) << 16) |
                             ((ULONG)(Key[2]) << 8) |
                             Key[3];

        Key += 4;
    }

    //
    // Create the round keys.
    //

    CurrentRcon = (PUCHAR)CyAesRcon;
    ExpandedKeyRange = (Context->Rounds + 1) * 4;
    for (Index = Words; Index < ExpandedKeyRange; Index += 1) {
        KeyValue = LongPointer[Index - 1];
        if ((Index % Words) == 0) {
            WorkingKey = ((ULONG)CyAesSbox[KeyValue & 0xFF] << 8) |
                         ((ULONG)CyAesSbox[(KeyValue >> 8) & 0xFF] << 16) |
                         ((ULONG)CyAesSbox[(KeyValue >> 16) & 0xFF] << 24) |
                         ((ULONG)CyAesSbox[(KeyValue >> 24) & 0xFF]);

            KeyValue = WorkingKey ^ (((ULONG)*CurrentRcon) << 24);
            CurrentRcon += 1;
        }

        if ((Words == 8) && ((Index % Words) == 4)) {
            WorkingKey = ((ULONG)CyAesSbox[KeyValue & 0xFF]) |
                         ((ULONG)CyAesSbox[(KeyValue >> 8) & 0xFF] << 8) |
                         ((ULONG)CyAesSbox[(KeyValue >> 16) & 0xFF] << 16) |
                         ((ULONG)CyAesSbox[(KeyValue >> 24) & 0xFF]) << 24;

            KeyValue = WorkingKey;
        }

        LongPointer[Index] = LongPointer[Index - Words] ^ KeyValue;
    }

    //
    // Just copy the initialization vector straight over, ignoring it for ECB
    // modes.
    //

    if ((Mode != AesModeEcb128) && (Mode != AesModeEcb256)) {
        if (InitializationVector != NULL) {
            RtlCopyMemory(Context->InitializationVector,
                          InitializationVector,
                          AES_INITIALIZATION_VECTOR_SIZE);

        } else {
            RtlZeroMemory(Context->InitializationVector,
                          AES_INITIALIZATION_VECTOR_SIZE);
        }
    }

    return;
}

CRYPTO_API
VOID
CyAesConvertKeyForDecryption (
    PAES_CONTEXT Context
    )

/*++

Routine Description:

    This routine prepares the context for decryption by performing the
    necessary transformations on the round keys.

Arguments:

    Context - Supplies a pointer to the AES context.

Return Value:

    None.

--*/

{

    INT Index;
    PULONG KeyLong;
    ULONG KeyValue;
    ULONG Temporary1;
    ULONG Temporary2;
    ULONG Temporary3;
    ULONG Temporary4;

    KeyLong = (PULONG)&(Context->Keys[0]);
    KeyLong += 4;
    for (Index = Context->Rounds * 4; Index > 4; Index -= 1) {
        KeyValue = *KeyLong;
        KeyValue = AES_INVERSE_MIX_COLUMNS(KeyValue,
                                           Temporary1,
                                           Temporary2,
                                           Temporary3,
                                           Temporary4);

        *KeyLong = KeyValue;
        KeyLong += 1;
    }

    return;
}

CRYPTO_API
VOID
CyAesCbcEncrypt (
    PAES_CONTEXT Context,
    PUCHAR Plaintext,
    PUCHAR Ciphertext,
    INT Length
    )

/*++

Routine Description:

    This routine encrypts a byte sequence (with a block size of 16) using the
    AES cipher.

Arguments:

    Context - Supplies a pointer to the AES context.

    Plaintext - Supplies a pointer to the plaintext buffer.

    Ciphertext - Supplies a pointer where the ciphertext will be returned.

    Length - Supplies the length of the plaintext and ciphertext buffers, in
        bytes. This length must be a multiple of 16 bytes.

Return Value:

    None.

--*/

{

    ULONG BlockIn[AES_BLOCK_SIZE / sizeof(ULONG)];
    ULONG BlockOut[AES_BLOCK_SIZE / sizeof(ULONG)];
    ULONG InitializationVector[AES_INITIALIZATION_VECTOR_SIZE / sizeof(ULONG)];
    PULONG InLong;
    PULONG OutLong;
    INT TextIndex;
    INT WordIndex;

    ASSERT((Length % AES_BLOCK_SIZE) == 0);

    RtlCopyMemory(InitializationVector,
                  Context->InitializationVector,
                  AES_INITIALIZATION_VECTOR_SIZE);

    //
    // Copy the initialization vector into the initial output block.
    //

    for (WordIndex = 0;
         WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
         WordIndex += 1) {

        BlockOut[WordIndex] = AES_BYTE_SWAP32(InitializationVector[WordIndex]);
    }

    //
    // Loop over and encrypt each block.
    //

    for (TextIndex = Length - AES_BLOCK_SIZE;
         TextIndex >= 0;
         TextIndex -= AES_BLOCK_SIZE) {

        InLong = (PULONG)Plaintext;
        OutLong = (PULONG)Ciphertext;
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            BlockIn[WordIndex] = AES_BYTE_SWAP32(InLong[WordIndex]) ^
                                 BlockOut[WordIndex];
        }

        CypAesEncryptBlock(Context, BlockIn);
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            BlockOut[WordIndex] = BlockIn[WordIndex];
            OutLong[WordIndex] = AES_BYTE_SWAP32(BlockIn[WordIndex]);
        }

        Plaintext += AES_BLOCK_SIZE;
        Ciphertext += AES_BLOCK_SIZE;
    }

    //
    // Copy the initialization vector back in to the context.
    //

    for (WordIndex = 0;
         WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
         WordIndex += 1) {

        InitializationVector[WordIndex] = AES_BYTE_SWAP32(BlockOut[WordIndex]);
    }

    RtlCopyMemory(Context->InitializationVector,
                  InitializationVector,
                  AES_INITIALIZATION_VECTOR_SIZE);

    return;
}

CRYPTO_API
VOID
CyAesCbcDecrypt (
    PAES_CONTEXT Context,
    PUCHAR Ciphertext,
    PUCHAR Plaintext,
    INT Length
    )

/*++

Routine Description:

    This routine decrypts a byte sequence (with a block size of 16) using the
    AES cipher.

Arguments:

    Context - Supplies a pointer to the AES context.

    Ciphertext - Supplies a pointer to the ciphertext buffer.

    Plaintext - Supplies a pointer where the plaintext will be returned.

    Length - Supplies the length of the plaintext and ciphertext buffers, in
        bytes. This length must be a multiple of 16 bytes.

Return Value:

    None.

--*/

{

    ULONG BlockIn[AES_BLOCK_SIZE / sizeof(ULONG)];
    ULONG BlockOut[AES_BLOCK_SIZE / sizeof(ULONG)];
    ULONG InitializationVector[AES_INITIALIZATION_VECTOR_SIZE / sizeof(ULONG)];
    PULONG InLong;
    PULONG OutLong;
    INT TextIndex;
    INT WordIndex;
    ULONG WorkingBlock[AES_BLOCK_SIZE / sizeof(ULONG)];
    ULONG XorValue[AES_BLOCK_SIZE / sizeof(ULONG)];

    ASSERT((Length % AES_BLOCK_SIZE) == 0);

    RtlCopyMemory(InitializationVector,
                  Context->InitializationVector,
                  AES_INITIALIZATION_VECTOR_SIZE);

    //
    // Copy the initialization vector into the initial XOR values.
    //

    for (WordIndex = 0;
         WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
         WordIndex += 1) {

        XorValue[WordIndex] = AES_BYTE_SWAP32(InitializationVector[WordIndex]);
    }

    //
    // Decrypt each block.
    //

    for (TextIndex = Length - AES_BLOCK_SIZE;
         TextIndex >= 0;
         TextIndex -= AES_BLOCK_SIZE) {

        InLong = (PULONG)Ciphertext;
        OutLong = (PULONG)Plaintext;
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            BlockIn[WordIndex] = AES_BYTE_SWAP32(InLong[WordIndex]);
            WorkingBlock[WordIndex] = BlockIn[WordIndex];
        }

        CypAesDecryptBlock(Context, WorkingBlock);
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            BlockOut[WordIndex] = WorkingBlock[WordIndex] ^ XorValue[WordIndex];
            XorValue[WordIndex] = BlockIn[WordIndex];
            OutLong[WordIndex] = AES_BYTE_SWAP32(BlockOut[WordIndex]);
        }

        Ciphertext += AES_BLOCK_SIZE;
        Plaintext += AES_BLOCK_SIZE;
    }

    //
    // Copy the initialization vector back in to the context.
    //

    for (WordIndex = 0;
         WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
         WordIndex += 1) {

        InitializationVector[WordIndex] = AES_BYTE_SWAP32(XorValue[WordIndex]);
    }

    RtlCopyMemory(Context->InitializationVector,
                  InitializationVector,
                  AES_INITIALIZATION_VECTOR_SIZE);

    return;
}

CRYPTO_API
VOID
CyAesEcbEncrypt (
    PAES_CONTEXT Context,
    PUCHAR Plaintext,
    PUCHAR Ciphertext,
    INT Length
    )

/*++

Routine Description:

    This routine encrypts a byte sequence (with a block size of 16) using the
    AES codebook.

Arguments:

    Context - Supplies a pointer to the AES context.

    Plaintext - Supplies a pointer to the plaintext buffer.

    Ciphertext - Supplies a pointer where the ciphertext will be returned.

    Length - Supplies the length of the plaintext and ciphertext buffers, in
        bytes. This length must be a multiple of 16 bytes.

Return Value:

    None.

--*/

{

    ULONG BlockIn[AES_BLOCK_SIZE / sizeof(ULONG)];
    PULONG InLong;
    PULONG OutLong;
    INT TextIndex;
    INT WordIndex;

    ASSERT((Length % AES_BLOCK_SIZE) == 0);

    //
    // Loop over and encrypt each block.
    //

    for (TextIndex = Length - AES_BLOCK_SIZE;
         TextIndex >= 0;
         TextIndex -= AES_BLOCK_SIZE) {

        InLong = (PULONG)Plaintext;
        OutLong = (PULONG)Ciphertext;
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            BlockIn[WordIndex] = AES_BYTE_SWAP32(InLong[WordIndex]);
        }

        CypAesEncryptBlock(Context, BlockIn);
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            OutLong[WordIndex] = AES_BYTE_SWAP32(BlockIn[WordIndex]);
        }

        Plaintext += AES_BLOCK_SIZE;
        Ciphertext += AES_BLOCK_SIZE;
    }

    return;
}

CRYPTO_API
VOID
CyAesEcbDecrypt (
    PAES_CONTEXT Context,
    PUCHAR Ciphertext,
    PUCHAR Plaintext,
    INT Length
    )

/*++

Routine Description:

    This routine decrypts a byte sequence (with a block size of 16) using the
    AES codebook.

Arguments:

    Context - Supplies a pointer to the AES context.

    Ciphertext - Supplies a pointer to the ciphertext buffer.

    Plaintext - Supplies a pointer where the plaintext will be returned.

    Length - Supplies the length of the plaintext and ciphertext buffers, in
        bytes. This length must be a multiple of 16 bytes.

Return Value:

    None.

--*/

{

    ULONG BlockIn[AES_BLOCK_SIZE / sizeof(ULONG)];
    PULONG InLong;
    PULONG OutLong;
    INT TextIndex;
    INT WordIndex;

    ASSERT((Length % AES_BLOCK_SIZE) == 0);

    //
    // Decrypt each block.
    //

    for (TextIndex = Length - AES_BLOCK_SIZE;
         TextIndex >= 0;
         TextIndex -= AES_BLOCK_SIZE) {

        InLong = (PULONG)Ciphertext;
        OutLong = (PULONG)Plaintext;
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            BlockIn[WordIndex] = AES_BYTE_SWAP32(InLong[WordIndex]);
        }

        CypAesDecryptBlock(Context, BlockIn);
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            OutLong[WordIndex] = AES_BYTE_SWAP32(BlockIn[WordIndex]);
        }

        Ciphertext += AES_BLOCK_SIZE;
        Plaintext += AES_BLOCK_SIZE;
    }

    return;
}

CRYPTO_API
VOID
CyAesCtrEncrypt (
    PAES_CONTEXT Context,
    PUCHAR Plaintext,
    PUCHAR Ciphertext,
    INT Length
    )

/*++

Routine Description:

    This routine encrypts a byte sequence (with a block size of 16) using AES
    counter mode.

Arguments:

    Context - Supplies a pointer to the AES context.

    Plaintext - Supplies a pointer to the plaintext buffer.

    Ciphertext - Supplies a pointer where the ciphertext will be returned.

    Length - Supplies the length of the plaintext and ciphertext buffers, in
        bytes. This length must be a multiple of 16 bytes.

Return Value:

    None.

--*/

{

    ULONG BlockIn[AES_BLOCK_SIZE / sizeof(ULONG)];
    INT ByteIndex;
    ULONG Counter[AES_INITIALIZATION_VECTOR_SIZE / sizeof(ULONG)];
    PUCHAR CounterBytes;
    PULONG InLong;
    PULONG OutLong;
    INT TextIndex;
    INT WordIndex;

    ASSERT((Length % AES_BLOCK_SIZE) == 0);

    RtlCopyMemory(Counter,
                  Context->InitializationVector,
                  AES_INITIALIZATION_VECTOR_SIZE);

    //
    // Encrypt the incrementing counter each iteration and XOR it with the next
    // block of input.
    //

    for (TextIndex = Length - AES_BLOCK_SIZE;
         TextIndex >= 0;
         TextIndex -= AES_BLOCK_SIZE) {

        //
        // Copy the counter into the input block.
        //

        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            BlockIn[WordIndex] = AES_BYTE_SWAP32(Counter[WordIndex]);
        }

        InLong = (PULONG)Plaintext;
        OutLong = (PULONG)Ciphertext;
        CypAesEncryptBlock(Context, BlockIn);
        for (WordIndex = 0;
             WordIndex < (AES_BLOCK_SIZE / sizeof(ULONG));
             WordIndex += 1) {

            OutLong[WordIndex] = AES_BYTE_SWAP32(BlockIn[WordIndex]) ^
                                 InLong[WordIndex];
        }

        Plaintext += AES_BLOCK_SIZE;
        Ciphertext += AES_BLOCK_SIZE;

        //
        // Increment the counter. Remember that this is big-endian.
        //

        CounterBytes = (PUCHAR)Counter;
        for (ByteIndex = AES_BLOCK_SIZE - 1;
             ByteIndex >= 0;
             ByteIndex -= 1) {

            CounterBytes[ByteIndex] += 1;
            if (CounterBytes[ByteIndex] != 0) {
                break;
            }
        }
    }

    //
    // Copy the counter back in to the context.
    //

    RtlCopyMemory(Context->InitializationVector,
                  Counter,
                  AES_INITIALIZATION_VECTOR_SIZE);

    return;
}

CRYPTO_API
VOID
CyAesCtrDecrypt (
    PAES_CONTEXT Context,
    PUCHAR Ciphertext,
    PUCHAR Plaintext,
    INT Length
    )

/*++

Routine Description:

    This routine decrypts a byte sequence (with a block size of 16) using AES
    counter mode.

Arguments:

    Context - Supplies a pointer to the AES context.

    Ciphertext - Supplies a pointer to the ciphertext buffer.

    Plaintext - Supplies a pointer where the plaintext will be returned.

    Length - Supplies the length of the plaintext and ciphertext buffers, in
        bytes. This length must be a multiple of 16 bytes.

Return Value:

    None.

--*/

{

    //
    // Counter mode always uses AES encryption to derive a value from the
    // counter and then XOR's that value with the input. Thus, decryption is
    // the same as encryption except the ciphertext is the input and the
    // plaintext is the output.
    //

    CyAesCtrEncrypt(Context, Ciphertext, Plaintext, Length);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CypAesEncryptBlock (
    PAES_CONTEXT Context,
    PULONG Block
    )

/*++

Routine Description:

    This routine encrypts a single block of data using the AES cipher.

Arguments:

    Context - Supplies a pointer to the AES context.

    Block - Supplies a pointer to the block to encrypt.

Return Value:

    None. The encrypted data will be returned inline.

--*/

{

    PULONG Key;
    UCHAR OriginalVector0;
    INT Round;
    INT Row;
    UCHAR SboxIndex;
    UCHAR Vector[4];
    ULONG WorkingBlock[4];
    ULONG XorAll;

    Key = (PULONG)(Context->Keys);

    //
    // Perform pre-round key addition.
    //

    for (Row = 0; Row < 4; Row += 1) {
        Block[Row] ^= *Key;
        Key += 1;
    }

    //
    // Loop through and encrypt the block.
    //

    for (Round = 0; Round < Context->Rounds; Round += 1) {
        for (Row = 0; Row < 4; Row += 1) {

            //
            // Perform the byte substitution and row shift operations together.
            //

            SboxIndex = (UCHAR)(Block[Row % 4] >> 24);
            Vector[0] = CyAesSbox[SboxIndex];
            SboxIndex = (UCHAR)(Block[(Row + 1) % 4] >> 16);
            Vector[1] = CyAesSbox[SboxIndex];
            SboxIndex = (UCHAR)(Block[(Row + 2) % 4] >> 8);
            Vector[2] = CyAesSbox[SboxIndex];
            SboxIndex = (UCHAR)(Block[(Row + 3) % 4]);
            Vector[3] = CyAesSbox[SboxIndex];

            //
            // If this is not the last round, perform the mix columns operation.
            //

            if (Round != Context->Rounds - 1) {
                XorAll = Vector[0] ^ Vector[1] ^ Vector[2] ^ Vector[3];
                OriginalVector0 = Vector[0];
                Vector[0] ^= XorAll ^ CypAesXtime(Vector[0] ^ Vector[1]);
                Vector[1] ^= XorAll ^ CypAesXtime(Vector[1] ^ Vector[2]);
                Vector[2] ^= XorAll ^ CypAesXtime(Vector[2] ^ Vector[3]);
                Vector[3] ^= XorAll ^ CypAesXtime(Vector[3] ^ OriginalVector0);
            }

            WorkingBlock[Row] = ((ULONG)Vector[0] << 24) |
                                ((ULONG)Vector[1] << 16) |
                                ((ULONG)Vector[2] << 8) |
                                Vector[3];
        }

        //
        // Perform key addition now that the mix column operation is complete.
        //

        for (Row = 0; Row < 4; Row += 1) {
            Block[Row] = WorkingBlock[Row] ^ *Key;
            Key += 1;
        }
    }

    return;
}

VOID
CypAesDecryptBlock (
    PAES_CONTEXT Context,
    PULONG Block
    )

/*++

Routine Description:

    This routine decrypts a single block of data using the AES cipher.

Arguments:

    Context - Supplies a pointer to the AES context.

    Block - Supplies a pointer to the block to decrypt.

Return Value:

    None. The decrypted data will be returned inline.

--*/

{

    PULONG Key;
    INT Round;
    INT Row;
    UCHAR SboxIndex;
    UCHAR Vector[4];
    ULONG WorkingBlock[4];
    UCHAR XorValue0;
    UCHAR XorValue1;
    UCHAR XorValue2;
    UCHAR XorValue3;
    UCHAR XorValue4;
    UCHAR XorValue5;
    UCHAR XorValue6;

    Key = Context->Keys + ((Context->Rounds + 1) * 4);

    //
    // Perform pre-round key addition.
    //

    for (Row = 4; Row > 0; Row -= 1) {
        Key -= 1;
        Block[Row - 1] ^= *Key;
    }

    //
    // Loop through and decrypt the block.
    //

    for (Round = 0; Round < Context->Rounds; Round += 1) {
        for (Row = 4; Row > 0; Row -= 1) {

            //
            // Perform the byte substitution and row shift operations together.
            //

            SboxIndex = (UCHAR)(Block[(Row + 3) % 4] >> 24);
            Vector[0] = CyAesInvertedSbox[SboxIndex];
            SboxIndex = (UCHAR)(Block[(Row + 2) % 4] >> 16);
            Vector[1] = CyAesInvertedSbox[SboxIndex];
            SboxIndex = (UCHAR)(Block[(Row + 1) % 4] >> 8);
            Vector[2] = CyAesInvertedSbox[SboxIndex];
            SboxIndex = (UCHAR)(Block[Row % 4]);
            Vector[3] = CyAesInvertedSbox[SboxIndex];

            //
            // Perform the Mix Column operation if this is not the last round.
            //

            if (Round < (Context->Rounds - 1)) {
                XorValue0 = CypAesXtime(Vector[0] ^ Vector[1]);
                XorValue1 = CypAesXtime(Vector[1] ^ Vector[2]);
                XorValue2 = CypAesXtime(Vector[2] ^ Vector[3]);
                XorValue3 = CypAesXtime(Vector[3] ^ Vector[0]);
                XorValue4 = CypAesXtime(XorValue0 ^ XorValue1);
                XorValue5 = CypAesXtime(XorValue1 ^ XorValue2);
                XorValue6 = CypAesXtime(XorValue4 ^ XorValue5);
                XorValue0 ^= Vector[1] ^ Vector[2] ^ Vector[3] ^ XorValue4 ^
                             XorValue6;

                XorValue1 ^= Vector[0] ^ Vector[2] ^ Vector[3] ^ XorValue5 ^
                             XorValue6;

                XorValue2 ^= Vector[0] ^ Vector[1] ^ Vector[3] ^ XorValue4 ^
                             XorValue6;

                XorValue3 ^= Vector[0] ^ Vector[1] ^ Vector[2] ^ XorValue5 ^
                             XorValue6;

                WorkingBlock[Row - 1] = ((ULONG)XorValue0 << 24) |
                                        ((ULONG)XorValue1 << 16) |
                                        ((ULONG)XorValue2 << 8) |
                                        XorValue3;

            } else {
                WorkingBlock[Row - 1] = ((ULONG)Vector[0] << 24) |
                                        ((ULONG)Vector[1] << 16) |
                                        ((ULONG)Vector[2] << 8) |
                                        Vector[3];
            }
        }

        for (Row = 4; Row > 0; Row -= 1) {
            Key -= 1;
            Block[Row - 1] = WorkingBlock[Row - 1] ^ *Key;
        }
    }

    return;
}

UCHAR
CypAesXtime (
    UCHAR Value
    )

/*++

Routine Description:

    This routine performs doubling of an 8 bit value in a Galois Field GF(2^8)
    using the irreducible polynomial x^8 + x^4 + x^3 + x + 1. This basically
    means multiply by 2 and exclusive OR with 0x1B if it rolls over.

Arguments:

    Value - Supplies the value to multiply by 2 in the finite field.

Return Value:

    Returns the value multiplied by 2 in the finite field.

--*/

{

    if ((Value & 0x80) != 0) {
        return (Value << 1) ^ 0x1B;
    }

    return Value << 1;
}


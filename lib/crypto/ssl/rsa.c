/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rsa.c

Abstract:

    This module impelements support for the Rivest Shamir Adleman public/
    private key encryption scheme.

Author:

    Evan Green 21-Jul-2015

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

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PBIG_INTEGER
CypRsaRunPrivateKey (
    PRSA_CONTEXT Context,
    PBIG_INTEGER Message
    );

PBIG_INTEGER
CypRsaRunPublicKey (
    PRSA_CONTEXT Context,
    PBIG_INTEGER Message
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
KSTATUS
CyRsaInitializeContext (
    PRSA_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes an RSA context. The caller must have filled out
    the allocate, reallocate, and free memory routine pointers in the big
    integer context, and zeroed the rest of the structure.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = CypBiInitializeContext(&(Context->BigIntegerContext));
    if (!KSUCCESS(Status)) {
        return Status;
    }

    ASSERT(Context->ModulusSize == 0);

    return Status;
}

CRYPTO_API
VOID
CyRsaDestroyContext (
    PRSA_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a previously intialized RSA context.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

{

    PBIG_INTEGER_CONTEXT BigContext;
    PBIG_INTEGER Value;

    BigContext = &(Context->BigIntegerContext);
    Value = Context->PublicExponent;
    if (Value != NULL) {
        CypBiMakeNonPermanent(Value);
        CypBiReleaseReference(BigContext, Value);
    }

    CypBiReleaseModuli(BigContext, BIG_INTEGER_M_OFFSET);
    Value = Context->PrivateExponent;
    if (Value != NULL) {
        CypBiMakeNonPermanent(Value);
        CypBiReleaseReference(BigContext, Value);
    }

    Value = Context->PValue;
    if (Value != NULL) {
        CypBiReleaseModuli(BigContext, BIG_INTEGER_P_OFFSET);
    }

    Value = Context->QValue;
    if (Value != NULL) {
        CypBiReleaseModuli(BigContext, BIG_INTEGER_Q_OFFSET);
    }

    Value = Context->DpValue;
    if (Value != NULL) {
        CypBiMakeNonPermanent(Value);
        CypBiReleaseReference(BigContext, Value);
    }

    Value = Context->DqValue;
    if (Value != NULL) {
        CypBiMakeNonPermanent(Value);
        CypBiReleaseReference(BigContext, Value);
    }

    Value = Context->QInverse;
    if (Value != NULL) {
        CypBiMakeNonPermanent(Value);
        CypBiReleaseReference(BigContext, Value);
    }

    CypBiDestroyContext(BigContext);

    //
    // Zero out the whole context to be safe.
    //

    RtlZeroMemory(Context, sizeof(RSA_CONTEXT));
    return;
}

CRYPTO_API
KSTATUS
CyRsaLoadPrivateKey (
    PRSA_CONTEXT Context,
    PRSA_PRIVATE_KEY_COMPONENTS PrivateKey
    )

/*++

Routine Description:

    This routine adds private key information to the given RSA context.

Arguments:

    Context - Supplies a pointer to the context.

    PrivateKey - Supplies a pointer to the private key information. All fields
        are required, including the public key ones.

Return Value:

    Status code.

--*/

{

    PBIG_INTEGER_CONTEXT BigInteger;
    KSTATUS Status;
    PBIG_INTEGER Value;

    Status = CyRsaLoadPublicKey(Context, &(PrivateKey->PublicKey));
    if (!KSUCCESS(Status)) {
        return Status;
    }

    BigInteger = &(Context->BigIntegerContext);
    Value = CypBiImport(BigInteger,
                        PrivateKey->PrivateExponent,
                        PrivateKey->PrivateExponentLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    CypBiMakePermanent(Value);
    Context->PrivateExponent = Value;
    Value = CypBiImport(BigInteger,
                        PrivateKey->PValue,
                        PrivateKey->PValueLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Context->PValue = Value;
    Value = CypBiImport(BigInteger,
                        PrivateKey->QValue,
                        PrivateKey->QValueLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Context->QValue = Value;
    Value = CypBiImport(BigInteger,
                        PrivateKey->DpValue,
                        PrivateKey->DpValueLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    CypBiMakePermanent(Value);
    Context->DpValue = Value;
    Value = CypBiImport(BigInteger,
                        PrivateKey->DqValue,
                        PrivateKey->DqValueLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    CypBiMakePermanent(Value);
    Context->DqValue = Value;
    Value = CypBiImport(BigInteger,
                        PrivateKey->QInverse,
                        PrivateKey->QInverseLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    CypBiMakePermanent(Value);
    Context->QInverse = Value;
    Status = CypBiCalculateModuli(BigInteger,
                                  Context->PValue,
                                  BIG_INTEGER_P_OFFSET);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = CypBiCalculateModuli(BigInteger,
                                  Context->QValue,
                                  BIG_INTEGER_Q_OFFSET);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

CRYPTO_API
KSTATUS
CyRsaLoadPublicKey (
    PRSA_CONTEXT Context,
    PRSA_PUBLIC_KEY_COMPONENTS PublicKey
    )

/*++

Routine Description:

    This routine adds public key information to the given RSA context.
    This routine should not be called if private key information was already
    added.

Arguments:

    Context - Supplies a pointer to the context.

    PublicKey - Supplies a pointer to the public key information. All fields
        are required.

Return Value:

    Status code.

--*/

{

    PBIG_INTEGER_CONTEXT BigInteger;
    KSTATUS Status;
    PBIG_INTEGER Value;

    BigInteger = &(Context->BigIntegerContext);
    Context->ModulusSize = PublicKey->ModulusLength;
    Value = CypBiImport(BigInteger,
                        PublicKey->Modulus,
                        PublicKey->ModulusLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = CypBiCalculateModuli(BigInteger, Value, BIG_INTEGER_M_OFFSET);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    Value = CypBiImport(BigInteger,
                        PublicKey->PublicExponent,
                        PublicKey->PublicExponentLength);

    if (Value == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    CypBiMakePermanent(Value);
    Context->PublicExponent = Value;
    return STATUS_SUCCESS;
}

CRYPTO_API
INTN
CyRsaDecrypt (
    PRSA_CONTEXT Context,
    PVOID Ciphertext,
    PVOID Plaintext,
    BOOL IsDecryption
    )

/*++

Routine Description:

    This routine performs RSA decryption.

Arguments:

    Context - Supplies a pointer to the context.

    Ciphertext - Supplies a pointer to the ciphertext, which must be less than
        the size of the modulus minus 11.

    Plaintext - Supplies a pointer where the plaintext will be returned.

    IsDecryption - Supplies a boolean indicating if this is a decryption
        operation (TRUE) or a verify operation (FALSE).

Return Value:

    Returns the number of bytes that were originally encrypted on success.

    -1 on allocation failure.

--*/

{

    PBIG_INTEGER_CONTEXT BigContext;
    PUCHAR Block;
    UINTN BlockSize;
    PBIG_INTEGER CipherInteger;
    UINTN LastIndex;
    PBIG_INTEGER PlainInteger;
    INTN Size;
    KSTATUS Status;

    BigContext = &(Context->BigIntegerContext);
    Size = -1;
    BlockSize = Context->ModulusSize;

    ASSERT(BlockSize > 10);

    Block = BigContext->AllocateMemory(BlockSize);
    if (Block == NULL) {
        goto RsaDecryptEnd;
    }

    CipherInteger = CypBiImport(BigContext, Ciphertext, BlockSize);
    if (CipherInteger == NULL) {
        goto RsaDecryptEnd;
    }

    if (IsDecryption != FALSE) {
        PlainInteger = CypRsaRunPrivateKey(Context, CipherInteger);

    } else {
        PlainInteger = CypRsaRunPublicKey(Context, CipherInteger);
    }

    if (PlainInteger == NULL) {
        CypBiReleaseReference(BigContext, CipherInteger);
        goto RsaDecryptEnd;
    }

    Status = CypBiExport(BigContext, PlainInteger, Block, BlockSize);
    if (!KSUCCESS(Status)) {
        CypBiReleaseReference(BigContext, PlainInteger);
        goto RsaDecryptEnd;
    }

    //
    // There are at least 11 bytes of padding even for a zero length
    // message.
    //

    LastIndex = 11;

    //
    // PKCS1.5 encryption is padded with random bytes.
    //

    if (IsDecryption != FALSE) {
        while ((Block[LastIndex] != 0) && (LastIndex < BlockSize)) {
            LastIndex += 1;
        }

    //
    // PKCS1.5 signing is padded with 0xFF.
    //

    } else {
        while ((Block[LastIndex] == 0xFF) && (LastIndex < BlockSize)) {
            LastIndex += 1;
        }

        if (Block[LastIndex - 1] != 0xFF) {
            LastIndex = BlockSize - 1;
        }
    }

    LastIndex += 1;
    Size = BlockSize - LastIndex;
    if (Size > 0) {
        RtlCopyMemory(Plaintext, &(Block[LastIndex]), Size);
    }

RsaDecryptEnd:
    if (Block != NULL) {
        BigContext->FreeMemory(Block);
    }

    return Size;
}

CRYPTO_API
INTN
CyRsaEncrypt (
    PRSA_CONTEXT Context,
    PVOID Plaintext,
    UINTN PlaintextLength,
    PVOID Ciphertext,
    BOOL IsSigning
    )

/*++

Routine Description:

    This routine performs RSA encryption.

Arguments:

    Context - Supplies a pointer to the context.

    Plaintext - Supplies a pointer to the plaintext to encrypt.

    PlaintextLength - Supplies the length of the plaintext buffer in bytes.

    Ciphertext - Supplies a pointer where the ciphertext will be returned.
        This buffer must be the size of the modulus.

    IsSigning - Supplies a boolean indicating whether this is a signing
        operation (TRUE) and should therefore use the private key, or
        whether this is an encryption operation (FALSE) and should use the
        public key.

Return Value:

    Returns the number of bytes that were originally encrypted on success. This
    is the same as the modulus size.

    -1 on allocation failure.

--*/

{

    PBIG_INTEGER_CONTEXT BigContext;
    PUCHAR Bytes;
    PBIG_INTEGER CipherInteger;
    UINTN PaddingBytes;
    PBIG_INTEGER PlainInteger;
    UINTN Size;
    KSTATUS Status;

    BigContext = &(Context->BigIntegerContext);
    Size = Context->ModulusSize;

    ASSERT(PlaintextLength <= Size - 3);

    PaddingBytes = Size - PlaintextLength - 3;

    //
    // Borrow the ciphertext buffer to construct the padded input buffer.
    //

    Bytes = Ciphertext;

    //
    // For PKCS1.5, signing pads with 0xFF bytes.
    //

    Bytes[0] = 0;
    if (IsSigning != FALSE) {
        Bytes[1] = 1;
        RtlSetMemory(&(Bytes[2]), 0xFF, PaddingBytes);

    //
    // Encryption pads with random bytes.
    //

    } else {
        Bytes[1] = 2;
        if (Context->FillRandom == NULL) {

            ASSERT(FALSE);

            return -1;
        }

        Context->FillRandom(&(Bytes[2]), PaddingBytes);
    }

    Bytes[2 + PaddingBytes] = 0;
    RtlCopyMemory(&(Bytes[2 + PaddingBytes + 1]), Plaintext, PlaintextLength);

    //
    // Make an integer from the padded plaintext.
    //

    PlainInteger = CypBiImport(BigContext, Ciphertext, Size);
    if (PlainInteger == NULL) {
        return -1;
    }

    if (IsSigning != FALSE) {
        CipherInteger = CypRsaRunPrivateKey(Context, PlainInteger);

    } else {
        CipherInteger = CypRsaRunPublicKey(Context, PlainInteger);
    }

    if (CipherInteger == NULL) {
        CypBiReleaseReference(BigContext, PlainInteger);
    }

    Status = CypBiExport(BigContext, CipherInteger, Ciphertext, Size);
    if (!KSUCCESS(Status)) {
        CypBiReleaseReference(BigContext, CipherInteger);
        return -1;
    }

    return Size;
}

//
// --------------------------------------------------------- Internal Functions
//

PBIG_INTEGER
CypRsaRunPrivateKey (
    PRSA_CONTEXT Context,
    PBIG_INTEGER Message
    )

/*++

Routine Description:

    This routine runs the given message integer through the private key,
    performing c^d mod n.

Arguments:

    Context - Supplies a pointer to the context.

    Message - Supplies a pointer to the message to encrypt/decrypt using the
        private key. A reference on this value will be released on success.

Return Value:

    Returns a pointer to the new value.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER Result;

    Result = CypBiChineseRemainderTheorem(&(Context->BigIntegerContext),
                                          Message,
                                          Context->DpValue,
                                          Context->DqValue,
                                          Context->PValue,
                                          Context->QValue,
                                          Context->QInverse);

    return Result;
}

PBIG_INTEGER
CypRsaRunPublicKey (
    PRSA_CONTEXT Context,
    PBIG_INTEGER Message
    )

/*++

Routine Description:

    This routine runs the given message integer through the public key,
    performing c^e mod n.

Arguments:

    Context - Supplies a pointer to the context.

    Message - Supplies a pointer to the message to encrypt/decrypt using the
        public key. A reference on this value will be released on success.

Return Value:

    Returns a pointer to the new value.

    NULL on allocation failure.

--*/

{

    PBIG_INTEGER Result;

    Context->BigIntegerContext.ModOffset = BIG_INTEGER_M_OFFSET;
    Result = CypBiExponentiateModulo(&(Context->BigIntegerContext),
                                     Message,
                                     Context->PublicExponent);

    return Result;
}


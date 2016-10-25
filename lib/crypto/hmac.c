/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hmac.c

Abstract:

    This module computes a Hashed Message Authentication Code based on a
    message, key, and hash function.

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
// ---------------------------------------------------------------- Definitions
//

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
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
VOID
CySha1ComputeHmac (
    PUCHAR Message,
    ULONG Length,
    PUCHAR Key,
    ULONG KeyLength,
    UCHAR Digest[SHA1_HASH_SIZE]
    )

/*++

Routine Description:

    This routine obtains a SHA-1 HMAC signature based on the message and key.
    Note that the key must be no longer than the hash function's block size.

Arguments:

    Message - Supplies a pointer to the data buffer to hash and sign.

    Length - Supplies the length of the message, in bytes.

    Key - Supplies a pointer to the secret key buffer.

    KeyLength - Supplies the length of the secret key, in bytes. This must be
        less than or equal to 64 bytes, the block size of the SHA-1 hash
        function.

    Digest - Supplies a pointer where the HMAC digest will be returned. This
        buffer is assumed to be 20 bytes, the size of a SHA-1 hash.

Return Value:

    None.

--*/

{

    INT ByteIndex;
    SHA1_CONTEXT Context;
    UCHAR Ipad[64];
    UCHAR Opad[64];

    //
    // Copy the key into ipad and opad, and pad any remainder with zero.
    //

    if (KeyLength > sizeof(Ipad)) {
        KeyLength = sizeof(Ipad);
    }

    for (ByteIndex = 0; ByteIndex < KeyLength; ByteIndex += 1) {
        Ipad[ByteIndex] = Key[ByteIndex];
        Opad[ByteIndex] = Key[ByteIndex];
    }

    while (ByteIndex < sizeof(Ipad)) {
        Ipad[ByteIndex] = 0;
        Opad[ByteIndex] = 0;
        ByteIndex += 1;
    }

    //
    // XOR in the magic values to ipad and opad.
    //

    for (ByteIndex = 0; ByteIndex < sizeof(Ipad); ByteIndex += 1) {
        Ipad[ByteIndex] ^= 0x36;
        Opad[ByteIndex] ^= 0x5C;
    }

    //
    // Perform the double hash.
    //

    CySha1Initialize(&Context);
    CySha1AddContent(&Context, Ipad, sizeof(Ipad));
    CySha1AddContent(&Context, Message, Length);
    CySha1GetHash(&Context, Digest);
    CySha1Initialize(&Context);
    CySha1AddContent(&Context, Opad, sizeof(Opad));
    CySha1AddContent(&Context, Digest, SHA1_HASH_SIZE);
    CySha1GetHash(&Context, Digest);
    return;
}

CRYPTO_API
VOID
CySha256ComputeHmac (
    PUCHAR Message,
    ULONG Length,
    PUCHAR Key,
    ULONG KeyLength,
    UCHAR Digest[SHA256_HASH_SIZE]
    )

/*++

Routine Description:

    This routine obtains a SHA-256 HMAC signature based on the message and key.
    Note that the key must be no longer than the hash function's block size.

Arguments:

    Message - Supplies a pointer to the data buffer to hash and sign.

    Length - Supplies the length of the message, in bytes.

    Key - Supplies a pointer to the secret key buffer.

    KeyLength - Supplies the length of the secret key, in bytes. This must be
        less than or equal to 64 bytes, the block size of the SHA-1 hash
        function.

    Digest - Supplies a pointer where the HMAC digest will be returned. This
        buffer is assumed to be 64 bytes, the size of a SHA-256 hash.

Return Value:

    None.

--*/

{

    INT ByteIndex;
    SHA256_CONTEXT Context;
    UCHAR Ipad[64];
    UCHAR Opad[64];

    //
    // Copy the key into ipad and opad, and pad any remainder with zero.
    //

    if (KeyLength > sizeof(Ipad)) {
        KeyLength = sizeof(Ipad);
    }

    for (ByteIndex = 0; ByteIndex < KeyLength; ByteIndex += 1) {
        Ipad[ByteIndex] = Key[ByteIndex];
        Opad[ByteIndex] = Key[ByteIndex];
    }

    while (ByteIndex < sizeof(Ipad)) {
        Ipad[ByteIndex] = 0;
        Opad[ByteIndex] = 0;
        ByteIndex += 1;
    }

    //
    // XOR in the magic values to ipad and opad.
    //

    for (ByteIndex = 0; ByteIndex < sizeof(Ipad); ByteIndex += 1) {
        Ipad[ByteIndex] ^= 0x36;
        Opad[ByteIndex] ^= 0x5C;
    }

    //
    // Perform the double hash.
    //

    CySha256Initialize(&Context);
    CySha256AddContent(&Context, Ipad, sizeof(Ipad));
    CySha256AddContent(&Context, Message, Length);
    CySha256GetHash(&Context, Digest);
    CySha256Initialize(&Context);
    CySha256AddContent(&Context, Opad, sizeof(Opad));
    CySha256AddContent(&Context, Digest, SHA256_HASH_SIZE);
    CySha256GetHash(&Context, Digest);
    return;
}

CRYPTO_API
VOID
CyMd5ComputeHmac (
    PUCHAR Message,
    ULONG Length,
    PUCHAR Key,
    ULONG KeyLength,
    UCHAR Digest[MD5_HASH_SIZE]
    )

/*++

Routine Description:

    This routine obtains an MD5 HMAC signature based on the message and key.
    Note that the key must be no longer than the hash function's block size.

Arguments:

    Message - Supplies a pointer to the data buffer to hash and sign.

    Length - Supplies the length of the message, in bytes.

    Key - Supplies a pointer to the secret key buffer.

    KeyLength - Supplies the length of the secret key, in bytes. This must be
        less than or equal to 64 bytes, the block size of the SHA-1 hash
        function.

    Digest - Supplies a pointer where the HMAC digest will be returned. This
        buffer is assumed to be 16 bytes, the size of an MD5 hash.

Return Value:

    None.

--*/

{

    INT ByteIndex;
    MD5_CONTEXT Context;
    UCHAR Ipad[64];
    UCHAR Opad[64];

    //
    // Copy the key into ipad and opad, and pad any remainder with zero.
    //

    if (KeyLength > sizeof(Ipad)) {
        KeyLength = sizeof(Ipad);
    }

    for (ByteIndex = 0; ByteIndex < KeyLength; ByteIndex += 1) {
        Ipad[ByteIndex] = Key[ByteIndex];
        Opad[ByteIndex] = Key[ByteIndex];
    }

    while (ByteIndex < sizeof(Ipad)) {
        Ipad[ByteIndex] = 0;
        Opad[ByteIndex] = 0;
        ByteIndex += 1;
    }

    //
    // XOR in the magic values to ipad and opad.
    //

    for (ByteIndex = 0; ByteIndex < sizeof(Ipad); ByteIndex += 1) {
        Ipad[ByteIndex] ^= 0x36;
        Opad[ByteIndex] ^= 0x5C;
    }

    //
    // Perform the double hash.
    //

    CyMd5Initialize(&Context);
    CyMd5AddContent(&Context, Ipad, sizeof(Ipad));
    CyMd5AddContent(&Context, Message, Length);
    CyMd5GetHash(&Context, Digest);
    CyMd5Initialize(&Context);
    CyMd5AddContent(&Context, Opad, sizeof(Opad));
    CyMd5AddContent(&Context, Digest, MD5_HASH_SIZE);
    CyMd5GetHash(&Context, Digest);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


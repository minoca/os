/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crypt.c

Abstract:

    This module implements the crypt library functions.

Author:

    Evan Green 6-Mar-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "cryptp.h"

#include <minoca/lib/minocaos.h>
#include <minoca/lib/crypto.h>

#include <alloca.h>
#include <errno.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define CRYPT_ALPHABET \
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

//
// The number of rounds in SHA-256 crypt can be specified.
//

#define CRYPT_SHA256_ROUNDS_DEFAULT 5000
#define CRYPT_SHA256_ROUNDS_MIN 1000
#define CRYPT_SHA256_ROUNDS_MAX 999999999

#define CRYPT_SHA256_SALT_MAX 16

//
// The number of rounds in SHA-512 crypt can be specified.
//

#define CRYPT_SHA512_ROUNDS_DEFAULT 5000
#define CRYPT_SHA512_ROUNDS_MIN 1000
#define CRYPT_SHA512_ROUNDS_MAX 999999999

#define CRYPT_SHA512_SALT_MAX 16

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
PSTR
(*PCRYPT_FUNCTION) (
    PSTR Key,
    PSTR Salt
    );

/*++

Routine Description:

    This routine defines the format for a crypt algorithm function.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies the ID and salt information.

Return Value:

    Returns a pointer to the encrypted password (plus ID and salt information
    in cases where an alternate mechanism is used). This is a static buffer,
    which may be overwritten by subsequent calls to crypt.

--*/

/*++

Structure Description:

    This structure stores the tuple of a crypt hashing algorithm's ID and
    function pointer.

Members:

    Name - Stores a pointer to a string containing the name of the algorithm.

    Id - Stores the ID string that needs to appear at the beginning of the
        salt to match this algorithm.

    CryptFunction - Stores a pointer to a function used to encrypt the data.

--*/

typedef struct _CRYPT_FORMAT {
    PSTR Name;
    PSTR Id;
    PCRYPT_FUNCTION CryptFunction;
} CRYPT_FORMAT, *PCRYPT_FORMAT;

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
ClpCryptMd5 (
    PSTR Key,
    PSTR Salt
    );

PSTR
ClpCryptSha256 (
    PSTR Key,
    PSTR Salt
    );

PSTR
ClpCryptSha256Reentrant (
    PSTR Key,
    PSTR Salt,
    PSTR Buffer,
    UINTN BufferLength
    );

PSTR
ClpCryptSha512 (
    PSTR Key,
    PSTR Salt
    );

PSTR
ClpCryptSha512Reentrant (
    PSTR Key,
    PSTR Salt,
    PSTR Buffer,
    UINTN BufferLength
    );

VOID
ClpCryptConvertToCharacters (
    PSTR *StringPointer,
    UCHAR ValueHigh,
    UCHAR ValueMid,
    UCHAR ValueLow,
    INTN Size,
    PUINTN BufferLength
    );

//
// -------------------------------------------------------------------- Globals
//

CRYPT_FORMAT ClCryptFormats[] = {
    {"md5", "$1$", ClpCryptMd5},
    {"sha256", "$5$", ClpCryptSha256},
    {"sha512", "$6$", ClpCryptSha512},
    {NULL, NULL, NULL}
};

//
// Store the static buffer containing crypt results.
//

char ClCryptBuffer[120];

//
// ------------------------------------------------------------------ Functions
//

LIBCRYPT_API
char *
crypt (
    const char *Key,
    const char *Salt
    )

/*++

Routine Description:

    This routine encrypts a user's password using various encryption/hashing
    standards. The default is DES, which is fairly weak and subject to
    dictionary attacks.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies a two character salt to use to perterb the results. If this
        string starts with a $ and a number, alternate hashing algorithms are
        selected. The format is $id$salt$encrypted. ID can be 1 for MD5, 5 for
        SHA-256, or 6 for SHA-512.

Return Value:

    Returns a pointer to the encrypted password (plus ID and salt information
    in cases where an alternate mechanism is used). This is a static buffer,
    which may be overwritten by subsequent calls to crypt.

--*/

{

    PCRYPT_FORMAT Format;

    Format = ClCryptFormats;
    while (Format->CryptFunction != NULL) {
        if ((Format->Id != NULL) && (strstr(Salt, Format->Id) == Salt)) {
            return Format->CryptFunction((PSTR)Key, (PSTR)Salt);
        }

        Format += 1;
    }

    return ClpCryptSha512((PSTR)Key, (PSTR)Salt);
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
ClpCryptMd5 (
    PSTR Key,
    PSTR Salt
    )

/*++

Routine Description:

    This routine encrypts a user's password using the MD5 hash algorithm.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies the ID and salt information.

Return Value:

    Returns a pointer to the encrypted password, plus ID and salt information.
    This is a static buffer, which may be overwritten by subsequent calls to
    crypt.

--*/

{

    int Bits;
    UINTN BufferSize;
    MD5_CONTEXT Context;
    MD5_CONTEXT Context2;
    UCHAR Hash[MD5_HASH_SIZE];
    size_t HashLength;
    PSTR Id;
    size_t IdLength;
    int Iteration;
    size_t KeyLength;
    char *Result;
    const char *SaltEnd;
    size_t SaltLength;

    Id = "$1$";
    IdLength = strlen(Id);
    KeyLength = strlen(Key);

    //
    // Skip the ID part of the salt.
    //

    if (strncmp(Salt, Id, IdLength) == 0) {
        Salt += IdLength;
    }

    //
    // Compute the salt length, capped at 8 characters.
    //

    SaltEnd = Salt;
    SaltLength = 0;
    while ((*SaltEnd != '\0') && (*SaltEnd != '$') && (SaltLength < 8)) {
        SaltLength += 1;
        SaltEnd += 1;
    }

    //
    // Add the password, the magic string, and the salt.
    //

    CyMd5Initialize(&Context);
    CyMd5AddContent(&Context, Key, KeyLength);
    CyMd5AddContent(&Context, Id, IdLength);
    CyMd5AddContent(&Context, Salt, SaltLength);

    //
    // Take the MD5 of password, salt, password, and add in that hash for an
    // amount that corresponds to the length of the password.
    //

    CyMd5Initialize(&Context2);
    CyMd5AddContent(&Context2, Key, KeyLength);
    CyMd5AddContent(&Context2, Salt, SaltLength);
    CyMd5AddContent(&Context2, Key, KeyLength);
    CyMd5GetHash(&Context2, Hash);
    for (HashLength = KeyLength;
         HashLength >= MD5_HASH_SIZE;
         HashLength -= MD5_HASH_SIZE) {

        CyMd5AddContent(&Context, Hash, MD5_HASH_SIZE);
    }

    CyMd5AddContent(&Context, Hash, HashLength);

    //
    // Don't leave security treasures floating around.
    //

    memset(Hash, 0, MD5_HASH_SIZE);

    //
    // Add in either a zero or the first character of the password depending
    // on how bits in the length of the password are set.
    //

    Bits = KeyLength;
    while (Bits != 0) {
        if ((Bits & 0x1) != 0) {
            CyMd5AddContent(&Context, Hash, 1);

        } else {
            CyMd5AddContent(&Context, Key, 1);
        }

        Bits = Bits >> 1;
    }

    strcpy(ClCryptBuffer, Id);
    strncat(ClCryptBuffer, Salt, SaltLength);
    strcat(ClCryptBuffer, "$");
    CyMd5GetHash(&Context, Hash);

    //
    // Do some more iterations just to slow things down a little.
    //

    for (Iteration = 0; Iteration < 1000; Iteration += 1) {
        CyMd5Initialize(&Context2);
        if ((Iteration & 0x1) != 0) {
            CyMd5AddContent(&Context2, Key, KeyLength);

        } else {
            CyMd5AddContent(&Context2, Hash, MD5_HASH_SIZE);
        }

        if ((Iteration % 3) != 0) {
            CyMd5AddContent(&Context2, Salt, SaltLength);
        }

        if ((Iteration % 7) != 0) {
            CyMd5AddContent(&Context2, Key, KeyLength);
        }

        if ((Iteration & 0x1) != 0) {
            CyMd5AddContent(&Context2, Hash, MD5_HASH_SIZE);

        } else {
            CyMd5AddContent(&Context2, Key, KeyLength);
        }

        CyMd5GetHash(&Context2, Hash);
    }

    Result = ClCryptBuffer + strlen(ClCryptBuffer);
    BufferSize = sizeof(ClCryptBuffer) - ((UINTN)Result - (UINTN)ClCryptBuffer);
    ClpCryptConvertToCharacters(&Result,
                                Hash[0],
                                Hash[6],
                                Hash[12],
                                4,
                                &BufferSize);

    ClpCryptConvertToCharacters(&Result,
                                Hash[1],
                                Hash[7],
                                Hash[13],
                                4,
                                &BufferSize);

    ClpCryptConvertToCharacters(&Result,
                                Hash[2],
                                Hash[8],
                                Hash[14],
                                4,
                                &BufferSize);

    ClpCryptConvertToCharacters(&Result,
                                Hash[3],
                                Hash[9],
                                Hash[15],
                                4,
                                &BufferSize);

    ClpCryptConvertToCharacters(&Result,
                                Hash[4],
                                Hash[10],
                                Hash[5],
                                4,
                                &BufferSize);

    ClpCryptConvertToCharacters(&Result, 0, 0, Hash[11], 2, &BufferSize);
    *Result = '\0';

    //
    // No security droppings.
    //

    SECURITY_ZERO(Hash, MD5_HASH_SIZE);
    return ClCryptBuffer;
}

PSTR
ClpCryptSha256 (
    PSTR Key,
    PSTR Salt
    )

/*++

Routine Description:

    This routine encrypts a user's password using the SHA-256 hash algorithm.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies the ID and salt information.

Return Value:

    Returns a pointer to the encrypted password, plus ID and salt information.
    This is a static buffer, which may be overwritten by subsequent calls to
    crypt.

--*/

{

    PSTR Result;

    Result = ClpCryptSha256Reentrant(Key,
                                     Salt,
                                     ClCryptBuffer,
                                     sizeof(ClCryptBuffer));

    return Result;
}

PSTR
ClpCryptSha256Reentrant (
    PSTR Key,
    PSTR Salt,
    PSTR Buffer,
    UINTN BufferLength
    )

/*++

Routine Description:

    This routine encrypts a user's password using the SHA-256 hash algorithm.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies the ID and salt information.

    Buffer - Supplies a pointer where the result will be returned.

    BufferLength - Supplies the length of the buffer in bytes.

Return Value:

    Returns a pointer to the encrypted password, plus ID and salt information.

--*/

{

    PSTR AfterScan;
    UINTN Bits;
    SHA256_CONTEXT Context;
    SHA256_CONTEXT Context2;
    PSTR CurrentByte;
    UCHAR Hash[SHA256_HASH_SIZE];
    UCHAR Hash2[SHA256_HASH_SIZE];
    UINTN HashLength;
    PSTR Id;
    UINTN IdLength;
    UINTN Iteration;
    UINTN KeyLength;
    PSTR PBytes;
    UINTN Rounds;
    PSTR RoundsPrefix;
    UINTN RoundsPrefixLength;
    BOOL RoundsSpecified;
    PSTR RoundsString;
    UINTN SaltLength;
    UINTN SaltRounds;
    PSTR SBytes;
    UINTN StringLength;

    Id = "$5$";
    IdLength = strlen(Id);
    Rounds = CRYPT_SHA256_ROUNDS_DEFAULT;
    RoundsPrefix = "rounds=";
    RoundsPrefixLength = strlen(RoundsPrefix);
    RoundsSpecified = FALSE;

    //
    // Move over the salt ID.
    //

    if (strncmp(Salt, Id, IdLength) == 0) {
        Salt += IdLength;
    }

    if (strncmp(Salt, RoundsPrefix, RoundsPrefixLength) == 0) {
        RoundsString = Salt + RoundsPrefixLength;
        SaltRounds = strtoul(RoundsString, &AfterScan, 10);
        if (*AfterScan == '$') {
            Salt = AfterScan + 1;
            Rounds = SaltRounds;
            if (Rounds < CRYPT_SHA256_ROUNDS_MIN) {
                Rounds = CRYPT_SHA256_ROUNDS_MIN;

            } else if (Rounds > CRYPT_SHA256_ROUNDS_MAX) {
                Rounds = CRYPT_SHA256_ROUNDS_MAX;
            }

            RoundsSpecified = TRUE;
        }
    }

    SaltLength = strcspn(Salt, "$");
    if (SaltLength > CRYPT_SHA256_SALT_MAX) {
        SaltLength = CRYPT_SHA256_SALT_MAX;
    }

    KeyLength = strlen(Key);
    CySha256Initialize(&Context);
    CySha256AddContent(&Context, Key, KeyLength);
    CySha256AddContent(&Context, Salt, SaltLength);

    //
    // In a different context, add the key, salt, and key again.
    //

    CySha256Initialize(&Context2);
    CySha256AddContent(&Context2, Key, KeyLength);
    CySha256AddContent(&Context2, Salt, SaltLength);
    CySha256AddContent(&Context2, Key, KeyLength);
    CySha256GetHash(&Context2, Hash);

    //
    // For each character of the key, add the alternate sum.
    //

    for (HashLength = KeyLength;
         HashLength >= SHA256_HASH_SIZE;
         HashLength -= SHA256_HASH_SIZE) {

        CySha256AddContent(&Context, Hash, SHA256_HASH_SIZE);
    }

    CySha256AddContent(&Context, Hash, HashLength);

    //
    // For the bits in the key length, add in either the hash or the key,
    // depending on the bit value.
    //

    for (Bits = KeyLength; Bits > 0; Bits >>= 1) {
        if ((Bits & 0x1) != 0) {
            CySha256AddContent(&Context, Hash, SHA256_HASH_SIZE);

        } else {
            CySha256AddContent(&Context, Key, KeyLength);
        }
    }

    CySha256GetHash(&Context, Hash);

    //
    // Compute another alternate hash. For every byte in the password add the
    // password.
    //

    CySha256Initialize(&Context2);
    for (Iteration = 0; Iteration < KeyLength; Iteration += 1) {
        CySha256AddContent(&Context2, Key, KeyLength);
    }

    CySha256GetHash(&Context2, Hash2);

    //
    // Create a P-Sequence.
    //

    PBytes = alloca(KeyLength);
    CurrentByte = PBytes;
    for (HashLength = KeyLength;
         HashLength >= SHA256_HASH_SIZE;
         HashLength -= SHA256_HASH_SIZE) {

        memcpy(CurrentByte, Hash2, SHA256_HASH_SIZE);
        CurrentByte += SHA256_HASH_SIZE;
    }

    memcpy(CurrentByte, Hash2, HashLength);

    //
    // Begin computation of the S-Sequence.
    //

    CySha256Initialize(&Context2);
    for (Iteration = 0; Iteration < 16 + Hash[0]; Iteration += 1) {
        CySha256AddContent(&Context2, Salt, SaltLength);
    }

    CySha256GetHash(&Context2, Hash2);

    //
    // Create and compute the S-Sequence.
    //

    SBytes = alloca(SaltLength);
    CurrentByte = SBytes;
    for (HashLength = SaltLength;
         HashLength >= SHA256_HASH_SIZE;
         HashLength -= SHA256_HASH_SIZE) {

        memcpy(CurrentByte, Hash2, SHA256_HASH_SIZE);
    }

    memcpy(CurrentByte, Hash2, HashLength);

    //
    // Re-crunch the hash for the given rounds to make things computationally
    // expensive.
    //

    for (Iteration = 0; Iteration < Rounds; Iteration += 1) {
        CySha256Initialize(&Context);
        if ((Iteration & 0x1) != 0) {
            CySha256AddContent(&Context, PBytes, KeyLength);

        } else {
            CySha256AddContent(&Context, Hash, SHA256_HASH_SIZE);
        }

        if ((Iteration % 3) != 0) {
            CySha256AddContent(&Context, SBytes, SaltLength);
        }

        if ((Iteration % 7) != 0) {
            CySha256AddContent(&Context, PBytes, KeyLength);
        }

        if ((Iteration & 0x1) != 0) {
            CySha256AddContent(&Context, Hash, SHA256_HASH_SIZE);

        } else {
            CySha256AddContent(&Context, PBytes, KeyLength);
        }

        CySha256GetHash(&Context, Hash);
    }

    //
    // The heavy lifting is done. Start to create the output string.
    //

    CurrentByte = stpncpy(Buffer, Id, BufferLength);
    if (BufferLength >= IdLength) {
        BufferLength -= IdLength;

    } else {
        BufferLength = 0;
    }

    if (RoundsSpecified != FALSE) {
        StringLength = snprintf(CurrentByte,
                                BufferLength,
                                "%s%zu$",
                                RoundsPrefix,
                                Rounds);

        CurrentByte += StringLength;
        if (BufferLength >= StringLength) {
            BufferLength -= StringLength;

        } else {
            BufferLength = 0;
        }
    }

    CurrentByte = stpncpy(CurrentByte, Salt, MIN(BufferLength, SaltLength));
    if (BufferLength >= SaltLength) {
        BufferLength -= SaltLength;

    } else {
        BufferLength = 0;
    }

    if (BufferLength > 0) {
        *CurrentByte = '$';
        CurrentByte += 1;
        BufferLength -= 1;
    }

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[0],
                                Hash[10],
                                Hash[20],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[21],
                                Hash[1],
                                Hash[11],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[12],
                                Hash[22],
                                Hash[2],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[3],
                                Hash[13],
                                Hash[23],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[24],
                                Hash[4],
                                Hash[14],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[15],
                                Hash[25],
                                Hash[5],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[6],
                                Hash[16],
                                Hash[26],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[27],
                                Hash[7],
                                Hash[17],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[18],
                                Hash[28],
                                Hash[8],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[9],
                                Hash[19],
                                Hash[29],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                0,
                                Hash[31],
                                Hash[30],
                                3,
                                &BufferLength);

    if (BufferLength == 0) {
        errno = ERANGE;
        Buffer = NULL;

    } else {
        *CurrentByte = '\0';
    }

    //
    // Clear things out to avoid leaving security context around.
    //

    SECURITY_ZERO(Hash, SHA256_HASH_SIZE);
    SECURITY_ZERO(Hash2, SHA256_HASH_SIZE);
    SECURITY_ZERO(PBytes, KeyLength);
    SECURITY_ZERO(SBytes, SaltLength);
    SECURITY_ZERO(&Context, sizeof(Context));
    SECURITY_ZERO(&Context2, sizeof(Context2));
    return Buffer;
}

PSTR
ClpCryptSha512 (
    PSTR Key,
    PSTR Salt
    )

/*++

Routine Description:

    This routine encrypts a user's password using the SHA-512 hash algorithm.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies the ID and salt information.

Return Value:

    Returns a pointer to the encrypted password, plus ID and salt information.
    This is a static buffer, which may be overwritten by subsequent calls to
    crypt.

--*/

{

    PSTR Result;

    Result = ClpCryptSha512Reentrant(Key,
                                     Salt,
                                     ClCryptBuffer,
                                     sizeof(ClCryptBuffer));

    return Result;
}

PSTR
ClpCryptSha512Reentrant (
    PSTR Key,
    PSTR Salt,
    PSTR Buffer,
    UINTN BufferLength
    )

/*++

Routine Description:

    This routine encrypts a user's password using the SHA-512 hash algorithm.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies the ID and salt information.

    Buffer - Supplies a pointer where the result will be returned.

    BufferLength - Supplies the length of the buffer in bytes.

Return Value:

    Returns a pointer to the encrypted password, plus ID and salt information.

--*/

{

    PSTR AfterScan;
    UINTN Bits;
    SHA512_CONTEXT Context;
    SHA512_CONTEXT Context2;
    PSTR CurrentByte;
    UCHAR Hash[SHA512_HASH_SIZE];
    UCHAR Hash2[SHA512_HASH_SIZE];
    UINTN HashLength;
    PSTR Id;
    UINTN IdLength;
    UINTN Iteration;
    UINTN KeyLength;
    PSTR PBytes;
    UINTN Rounds;
    PSTR RoundsPrefix;
    UINTN RoundsPrefixLength;
    BOOL RoundsSpecified;
    PSTR RoundsString;
    UINTN SaltLength;
    UINTN SaltRounds;
    PSTR SBytes;
    UINTN StringLength;

    Id = "$6$";
    IdLength = strlen(Id);
    Rounds = CRYPT_SHA512_ROUNDS_DEFAULT;
    RoundsPrefix = "rounds=";
    RoundsPrefixLength = strlen(RoundsPrefix);
    RoundsSpecified = FALSE;

    //
    // Move over the salt ID.
    //

    if (strncmp(Salt, Id, IdLength) == 0) {
        Salt += IdLength;
    }

    if (strncmp(Salt, RoundsPrefix, RoundsPrefixLength) == 0) {
        RoundsString = Salt + RoundsPrefixLength;
        SaltRounds = strtoul(RoundsString, &AfterScan, 10);
        if (*AfterScan == '$') {
            Salt = AfterScan + 1;
            Rounds = SaltRounds;
            if (Rounds < CRYPT_SHA512_ROUNDS_MIN) {
                Rounds = CRYPT_SHA512_ROUNDS_MIN;

            } else if (Rounds > CRYPT_SHA512_ROUNDS_MAX) {
                Rounds = CRYPT_SHA512_ROUNDS_MAX;
            }

            RoundsSpecified = TRUE;
        }
    }

    SaltLength = strcspn(Salt, "$");
    if (SaltLength > CRYPT_SHA512_SALT_MAX) {
        SaltLength = CRYPT_SHA512_SALT_MAX;
    }

    KeyLength = strlen(Key);
    CySha512Initialize(&Context);
    CySha512AddContent(&Context, Key, KeyLength);
    CySha512AddContent(&Context, Salt, SaltLength);

    //
    // In a different context, add the key, salt, and key again.
    //

    CySha512Initialize(&Context2);
    CySha512AddContent(&Context2, Key, KeyLength);
    CySha512AddContent(&Context2, Salt, SaltLength);
    CySha512AddContent(&Context2, Key, KeyLength);
    CySha512GetHash(&Context2, Hash);

    //
    // For each character of the key, add the alternate sum.
    //

    for (HashLength = KeyLength;
         HashLength > SHA512_HASH_SIZE;
         HashLength -= SHA512_HASH_SIZE) {

        CySha512AddContent(&Context, Hash, SHA512_HASH_SIZE);
    }

    CySha512AddContent(&Context, Hash, HashLength);

    //
    // For the bits in the key length, add in either the hash or the key,
    // depending on the bit value.
    //

    for (Bits = KeyLength; Bits > 0; Bits >>= 1) {
        if ((Bits & 0x1) != 0) {
            CySha512AddContent(&Context, Hash, SHA512_HASH_SIZE);

        } else {
            CySha512AddContent(&Context, Key, KeyLength);
        }
    }

    CySha512GetHash(&Context, Hash);

    //
    // Compute another alternate hash. For every byte in the password add the
    // password.
    //

    CySha512Initialize(&Context2);
    for (Iteration = 0; Iteration < KeyLength; Iteration += 1) {
        CySha512AddContent(&Context2, Key, KeyLength);
    }

    CySha512GetHash(&Context2, Hash2);

    //
    // Create a P-Sequence.
    //

    PBytes = alloca(KeyLength);
    CurrentByte = PBytes;
    for (HashLength = KeyLength;
         HashLength >= SHA512_HASH_SIZE;
         HashLength -= SHA512_HASH_SIZE) {

        memcpy(CurrentByte, Hash2, SHA512_HASH_SIZE);
        CurrentByte += SHA512_HASH_SIZE;
    }

    memcpy(CurrentByte, Hash2, HashLength);

    //
    // Begin computation of the S-Sequence.
    //

    CySha512Initialize(&Context2);
    for (Iteration = 0; Iteration < 16 + Hash[0]; Iteration += 1) {
        CySha512AddContent(&Context2, Salt, SaltLength);
    }

    CySha512GetHash(&Context2, Hash2);

    //
    // Create and compute the S-Sequence.
    //

    SBytes = alloca(SaltLength);
    CurrentByte = SBytes;
    for (HashLength = SaltLength;
         HashLength >= SHA512_HASH_SIZE;
         HashLength -= SHA512_HASH_SIZE) {

        memcpy(CurrentByte, Hash2, SHA512_HASH_SIZE);
    }

    memcpy(CurrentByte, Hash2, HashLength);

    //
    // Re-crunch the hash for the given rounds to make things computationally
    // expensive.
    //

    for (Iteration = 0; Iteration < Rounds; Iteration += 1) {
        CySha512Initialize(&Context);
        if ((Iteration & 0x1) != 0) {
            CySha512AddContent(&Context, PBytes, KeyLength);

        } else {
            CySha512AddContent(&Context, Hash, SHA512_HASH_SIZE);
        }

        if ((Iteration % 3) != 0) {
            CySha512AddContent(&Context, SBytes, SaltLength);
        }

        if ((Iteration % 7) != 0) {
            CySha512AddContent(&Context, PBytes, KeyLength);
        }

        if ((Iteration & 0x1) != 0) {
            CySha512AddContent(&Context, Hash, SHA512_HASH_SIZE);

        } else {
            CySha512AddContent(&Context, PBytes, KeyLength);
        }

        CySha512GetHash(&Context, Hash);
    }

    //
    // The heavy lifting is done. Start to create the output string.
    //

    CurrentByte = stpncpy(Buffer, Id, BufferLength);
    if (BufferLength >= IdLength) {
        BufferLength -= IdLength;

    } else {
        BufferLength = 0;
    }

    if (RoundsSpecified != FALSE) {
        StringLength = snprintf(CurrentByte,
                                BufferLength,
                                "%s%zu$",
                                RoundsPrefix,
                                Rounds);

        CurrentByte += StringLength;
        if (BufferLength >= StringLength) {
            BufferLength -= StringLength;

        } else {
            BufferLength = 0;
        }
    }

    CurrentByte = stpncpy(CurrentByte, Salt, MIN(BufferLength, SaltLength));
    if (BufferLength >= SaltLength) {
        BufferLength -= SaltLength;

    } else {
        BufferLength = 0;
    }

    if (BufferLength > 0) {
        *CurrentByte = '$';
        CurrentByte += 1;
        BufferLength -= 1;
    }

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[0],
                                Hash[21],
                                Hash[42],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[22],
                                Hash[43],
                                Hash[1],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[44],
                                Hash[2],
                                Hash[23],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[3],
                                Hash[24],
                                Hash[45],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[25],
                                Hash[46],
                                Hash[4],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[47],
                                Hash[5],
                                Hash[26],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[6],
                                Hash[27],
                                Hash[48],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[28],
                                Hash[49],
                                Hash[7],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[50],
                                Hash[8],
                                Hash[29],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[9],
                                Hash[30],
                                Hash[51],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[31],
                                Hash[52],
                                Hash[10],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[53],
                                Hash[11],
                                Hash[32],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[12],
                                Hash[33],
                                Hash[54],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[34],
                                Hash[55],
                                Hash[13],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[56],
                                Hash[14],
                                Hash[35],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[15],
                                Hash[36],
                                Hash[57],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[37],
                                Hash[58],
                                Hash[16],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[59],
                                Hash[17],
                                Hash[38],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[18],
                                Hash[39],
                                Hash[60],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[40],
                                Hash[61],
                                Hash[19],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                Hash[62],
                                Hash[20],
                                Hash[41],
                                4,
                                &BufferLength);

    ClpCryptConvertToCharacters(&CurrentByte,
                                0,
                                0,
                                Hash[63],
                                2,
                                &BufferLength);

    if (BufferLength == 0) {
        errno = ERANGE;
        Buffer = NULL;

    } else {
        *CurrentByte = '\0';
    }

    //
    // Clear things out to avoid leaving security context around.
    //

    SECURITY_ZERO(Hash, SHA512_HASH_SIZE);
    SECURITY_ZERO(Hash2, SHA512_HASH_SIZE);
    SECURITY_ZERO(PBytes, KeyLength);
    SECURITY_ZERO(SBytes, SaltLength);
    SECURITY_ZERO(&Context, sizeof(Context));
    SECURITY_ZERO(&Context2, sizeof(Context2));
    return Buffer;
}

VOID
ClpCryptConvertToCharacters (
    PSTR *StringPointer,
    UCHAR ValueHigh,
    UCHAR ValueMid,
    UCHAR ValueLow,
    INTN Size,
    PUINTN BufferLength
    )

/*++

Routine Description:

    This routine converts an integer into characters, 6 bits at a time.

Arguments:

    StringPointer - Supplies a pointer that on input contains a pointer where
        the characters will be returned. On output this value will be advanced.

    ValueHigh - Supplies the value from bits 16-23.

    ValueMid - Supplies the value from bits 8-15.

    ValueLow - Supplies the value from bits 0-7.

    Size - Supplies the number of characters to generate.

    BufferLength - Supplies a pointer that on input contains the remaining
        buffer space. On output this will be updated.

Return Value:

    None.

--*/

{

    PSTR String;
    ULONG Value;

    String = *StringPointer;
    Value = (ValueHigh << 16) | (ValueMid << 8) | ValueLow;
    while ((Size > 0) && (*BufferLength > 0)) {
        *String = CRYPT_ALPHABET[Value & 0x3F];
        String += 1;
        Value >>= 6;
        Size -= 1;
        *BufferLength -= 1;
    }

    *StringPointer = String;
    return;
}


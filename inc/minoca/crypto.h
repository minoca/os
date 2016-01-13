/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    crypto.h

Abstract:

    This header contains definitions for the Minoca cryptographic library.

Author:

    Evan Green 13-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#ifndef CRYPTO_API

#define CRYPTO_API DLLIMPORT

#endif

//
// Define AES parameters.
//

#define AES_MAX_ROUNDS 14
#define AES_BLOCK_SIZE 16
#define AES_INITIALIZATION_VECTOR_SIZE 16
#define AES_CBC128_KEY_SIZE 16
#define AES_CBC256_KEY_SIZE 32

//
// Define SHA-1 parameters.
//

#define SHA1_HASH_SIZE 20

//
// Define SHA-256 parameters.
//

#define SHA256_HASH_SIZE 32

//
// Define SHA-512 parameters.
//

#define SHA512_HASH_SIZE 64
#define SHA512_BLOCK_SIZE 128
#define SHA512_SHORT_BLOCK_SIZE (SHA512_BLOCK_SIZE - 16)

//
// Define MD5 parameters.
//

#define MD5_BLOCK_SIZE 64
#define MD5_HASH_SIZE 16

//
// Define Fortuna PRNG parameters.
//

#define FORTUNA_BLOCK_SIZE 16
#define FORTUNA_HASH_KEY_SIZE 32
#define FORTUNA_POOL_COUNT 23

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AES_CIPHER_MODE {
    AesModeInvalid,
    AesModeCbc128,
    AesModeCbc256,
} AES_CIPHER_MODE, *PAES_CIPHER_MODE;

typedef enum _FORTUNA_INITIALIZATION_STATE {
    FortunaNotInitialized,
    FortunaInitializationSeeded,
    FortunaInitialized
} FORTUNA_INITIALIZATION_STATE, *PFORTUNA_INITIALIZATION_STATE;

typedef
ULONGLONG
(*PCY_GET_TIME_COUNTER) (
    VOID
    );

/*++

Routine Description:

    This routine queries the time counter hardware and returns a 64-bit
    monotonically non-decreasing value that represents the number of timer
    ticks representing passage of time.

Arguments:

    None.

Return Value:

    Returns the number of timer ticks that have elapsed since some epoch value.

--*/

/*++

Structure Description:

    This structure stores the context used during AES encryption and decryption.

Members:

    Rounds - Stores the number of rounds used in this mode.

    KeySize - Stores the size of the key.

    Keys - Stores the initial key and each of the round keys.

    InitializationVector - Stores the initialization vector.

--*/

typedef struct _AES_CONTEXT {
    USHORT Rounds;
    USHORT KeySize;
    ULONG Keys[(AES_MAX_ROUNDS + 1) * 8];
    UCHAR InitializationVector[AES_INITIALIZATION_VECTOR_SIZE];
} AES_CONTEXT, *PAES_CONTEXT;

/*++

Structure Description:

    This structure stores the context used during computation of a SHA-1 hash.

Members:

    IntermediateHash - Stores the running digest.

    Length - Stores the length of the message, in bits.

    BlockIndex - Stores the current index into the message block array.

    MessageBlock - Stores the current block of the message being worked on.

--*/

typedef struct _SHA1_CONTEXT {
    ULONG IntermediateHash[SHA1_HASH_SIZE / sizeof(ULONG)];
    ULONGLONG Length;
    USHORT BlockIndex;
    UCHAR MessageBlock[64];
} SHA1_CONTEXT, *PSHA1_CONTEXT;

/*++

Structure Description:

    This structure stores the context used during computation of a SHA-256 hash.

Members:

    IntermediateHash - Stores the running digest.

    Length - Stores the length of the message, in bits.

    BlockIndex - Stores the current index into the message block array.

    MessageBlock - Stores the current block of the message being worked on.

--*/

typedef struct _SHA256_CONTEXT {
    ULONG IntermediateHash[SHA256_HASH_SIZE / sizeof(ULONG)];
    ULONGLONG Length;
    USHORT BlockIndex;
    UCHAR MessageBlock[64];
} SHA256_CONTEXT, *PSHA256_CONTEXT;

/*++

Structure Description:

    This structure stores the context used during computation of a SHA-512 hash.

Members:

    IntermediateHash - Stores the running digest.

    Length - Stores the length of the message, in bits.

    MessageBlock - Stores the current block of the message being worked on.

--*/

typedef struct _SHA512_CONTEXT {
    ULONGLONG IntermediateHash[SHA512_HASH_SIZE / sizeof(ULONGLONG)];
    ULONGLONG Length[2];
    UCHAR MessageBlock[SHA512_BLOCK_SIZE];
} SHA512_CONTEXT, *PSHA512_CONTEXT;

/*++

Structure Description:

    This structure stores the context used during computation of an MD5 hash.

Members:

    State - Stores the running digest.

    Length - Stores the length of the message, in bits.

    MessageBlock - Stores the current block of the message being worked on.

--*/

typedef struct _MD5_CONTEXT {
    ULONG State[4];
    ULONGLONG Length;
    UCHAR MessageBlock[MD5_BLOCK_SIZE];
} MD5_CONTEXT, *PMD5_CONTEXT;

/*++

Structure Description:

    This structure stores the context used by the Fortuna Pseudo-Random Number
    Generator.

Members:

    Counter - Stores the counter value, padded out to the cipher block size,
        for counting cipher blocks.

    Result - Stores the ciphertext result.

    Key - Stores the encryption key and hash.

    Pools - Stores the randomization source pools.

    CipherContext - Stores the encryption context.

    ReseedCount - Stores whether or not a reseed is needed.

    Pool0Bytes - Stores the number of bytes of entropy introduced into pool
        zero.

    Position - Stores a pool index where entropy is deposited.

    Initialized - Stores a state indicating whether the context is
        initialized or not.

    GetTimeCounter - Stores a pointer to a function used for retrieving the
        current time counter value.

    TimeCounterFrequency - Stores the frequency of the time counter, in Hertz.

    LastReseedTime - Stores the last time a reseed happened.

--*/

typedef struct _FORTUNA_CONTEXT {
    UCHAR Counter[FORTUNA_BLOCK_SIZE];
    UCHAR Result[FORTUNA_BLOCK_SIZE];
    UCHAR Key[FORTUNA_HASH_KEY_SIZE];
    SHA256_CONTEXT Pools[FORTUNA_POOL_COUNT];
    AES_CONTEXT CipherContext;
    UINTN ReseedCount;
    UINTN Pool0Bytes;
    UINTN Position;
    FORTUNA_INITIALIZATION_STATE Initialized;
    PCY_GET_TIME_COUNTER GetTimeCounter;
    ULONGLONG TimeCounterFrequency;
    ULONGLONG LastReseedTime;
} FORTUNA_CONTEXT, *PFORTUNA_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

CRYPTO_API
VOID
CyAesInitialize (
    PAES_CONTEXT Context,
    AES_CIPHER_MODE Mode,
    PUCHAR Key,
    PUCHAR InitializationVector
    );

/*++

Routine Description:

    This routine initializes an AES context structure, making it ready to
    encrypt and decrypt data.

Arguments:

    Context - Supplies a pointer to the AES state.

    Mode - Supplies the mode of AES to use.

    Key - Supplies the encryption/decryption key to use.

    InitializationVector - Supplies the initialization vector to start with.

Return Value:

    None. The AES context will be initialized and ready for operation.

--*/

CRYPTO_API
VOID
CyAesConvertKeyForDecryption (
    PAES_CONTEXT Context
    );

/*++

Routine Description:

    This routine prepares the context for decryption by performing the
    necessary transformations on the round keys.

Arguments:

    Context - Supplies a pointer to the AES context.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CyAesCbcEncrypt (
    PAES_CONTEXT Context,
    PUCHAR Plaintext,
    PUCHAR Ciphertext,
    INT Length
    );

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

CRYPTO_API
VOID
CyAesCbcDecrypt (
    PAES_CONTEXT Context,
    PUCHAR Ciphertext,
    PUCHAR Plaintext,
    INT Length
    );

/*++

Routine Description:

    This routine decrypts a byte sequence (with a block size of 16) using the
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

CRYPTO_API
VOID
CySha1ComputeHmac (
    PUCHAR Message,
    ULONG Length,
    PUCHAR Key,
    ULONG KeyLength,
    UCHAR Digest[SHA1_HASH_SIZE]
    );

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

CRYPTO_API
VOID
CySha256ComputeHmac (
    PUCHAR Message,
    ULONG Length,
    PUCHAR Key,
    ULONG KeyLength,
    UCHAR Digest[SHA256_HASH_SIZE]
    );

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

CRYPTO_API
VOID
CyMd5ComputeHmac (
    PUCHAR Message,
    ULONG Length,
    PUCHAR Key,
    ULONG KeyLength,
    UCHAR Digest[MD5_HASH_SIZE]
    );

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

CRYPTO_API
VOID
CySha1Initialize (
    PSHA1_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes a SHA-1 context structure, preparing it to accept
    and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CySha1AddContent (
    PSHA1_CONTEXT Context,
    PUCHAR Message,
    UINTN Length
    );

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

CRYPTO_API
VOID
CySha1GetHash (
    PSHA1_CONTEXT Context,
    UCHAR Hash[SHA1_HASH_SIZE]
    );

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

CRYPTO_API
VOID
CySha256Initialize (
    PSHA256_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes a SHA-256 context structure, preparing it to
    accept and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CySha256AddContent (
    PSHA256_CONTEXT Context,
    PVOID Message,
    UINTN Length
    );

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

CRYPTO_API
VOID
CySha256GetHash (
    PSHA256_CONTEXT Context,
    UCHAR Hash[SHA256_HASH_SIZE]
    );

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

CRYPTO_API
VOID
CySha512Initialize (
    PSHA512_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes a SHA-512 context structure, preparing it to
    accept and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CySha512AddContent (
    PSHA512_CONTEXT Context,
    PVOID Message,
    UINTN Length
    );

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

CRYPTO_API
VOID
CySha512GetHash (
    PSHA512_CONTEXT Context,
    UCHAR Hash[SHA512_HASH_SIZE]
    );

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

CRYPTO_API
VOID
CyMd5Initialize (
    PMD5_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes a MD5 context structure, preparing it to accept
    and hash data.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CyMd5AddContent (
    PMD5_CONTEXT Context,
    PVOID Message,
    UINTN Length
    );

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

CRYPTO_API
VOID
CyMd5GetHash (
    PMD5_CONTEXT Context,
    UCHAR Hash[MD5_HASH_SIZE]
    );

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

CRYPTO_API
VOID
CyFortunaInitialize (
    PFORTUNA_CONTEXT Context,
    PCY_GET_TIME_COUNTER GetTimeCounterFunction,
    ULONGLONG TimeCounterFrequency
    );

/*++

Routine Description:

    This routine initializes a Fortuna PRNG context. It does not seed it with
    any values.

Arguments:

    Context - Supplies a pointer to the context.

    GetTimeCounterFunction - Supplies an optional pointer to a function that
        can be used to retrieve a monotonically non-decreasing value
        representing the passage of time since some epoch.

    TimeCounterFrequency - Supplies the frequency of the time counter in
        Hertz.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CyFortunaGetRandomBytes (
    PFORTUNA_CONTEXT Context,
    PUCHAR Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine returns random bytes from a Fortuna instance.

Arguments:

    Context - Supplies a pointer to the context.

    Data - Supplies a pointer where the random bytes will be returned.

    Size - Supplies the number of bytes to return.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CyFortunaAddEntropy (
    PFORTUNA_CONTEXT Context,
    PVOID Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine adds random data into the mix.

Arguments:

    Context - Supplies a pointer to the context.

    Data - Supplies a pointer to the data to add.

    Size - Supplies the number of bytes of randomness in the data buffer.

Return Value:

    None.

--*/

//
// -------------------------------------------------------- Function Prototypes
//


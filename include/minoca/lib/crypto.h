/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#define CRYPTO_API __DLLIMPORT

#endif

//
// Define AES parameters.
//

#define AES_MAX_ROUNDS 14
#define AES_BLOCK_SIZE 16
#define AES_INITIALIZATION_VECTOR_SIZE 16
#define AES_CBC128_KEY_SIZE 16
#define AES_CBC256_KEY_SIZE 32
#define AES_ECB128_KEY_SIZE AES_CBC128_KEY_SIZE
#define AES_ECB256_KEY_SIZE AES_CBC256_KEY_SIZE
#define AES_CTR128_KEY_SIZE AES_CBC128_KEY_SIZE
#define AES_CTR256_KEY_SIZE AES_CBC256_KEY_SIZE

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
// Define big integer parameters.
//

#define BIG_INTEGER_MODULO_COUNT 3

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AES_CIPHER_MODE {
    AesModeInvalid,
    AesModeCbc128,
    AesModeCbc256,
    AesModeEcb128,
    AesModeEcb256,
    AesModeCtr128,
    AesModeCtr256
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
// Define functions called by the big integer library.
//

typedef
PVOID
(*PCY_ALLOCATE_MEMORY) (
    UINTN Size
    );

/*++

Routine Description:

    This routine is called when the crypto library needs to allocate memory.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

typedef
PVOID
(*PCY_REALLOCATE_MEMORY) (
    PVOID Allocation,
    UINTN Size
    );

/*++

Routine Description:

    This routine is called when the crypto library needs to adjust the size of
    a previous allocation.

Arguments:

    Allocation - Supplies the allocation to resize.

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

typedef
VOID
(*PCY_FREE_MEMORY) (
    PVOID Memory
    );

/*++

Routine Description:

    This routine is called when the crypto library needs to free allocated
    memory.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

typedef ULONG BIG_INTEGER_COMPONENT, *PBIG_INTEGER_COMPONENT;
typedef ULONGLONG BIG_INTEGER_LONG_COMPONENT, *PBIG_INTEGER_LONG_COMPONENT;
typedef struct _BIG_INTEGER BIG_INTEGER, *PBIG_INTEGER;

/*++

Structure Description:

    This structure stores a very large integer indeed.

Members:

    Next - Stores an optional pointer to the next big integer if this
        integer is on a list.

    Size - Stores the number of components in this integer.

    Capacity - Stores the number of components this allocation can
        sustain before the integer needs to be reallocated.

    ReferenceCount - Stores the reference count of the integer.

    Components - Stores a pointer to an array of integer components
        that make up the big integer.

--*/

struct _BIG_INTEGER {
    PBIG_INTEGER Next;
    USHORT Size;
    USHORT Capacity;
    LONG ReferenceCount;
    PBIG_INTEGER_COMPONENT Components;
};

/*++

Structure Description:

    This structure stores a big integer context, which maintains a
    cache of reusable big integers.

Members:

    AllocateMemory - Stores a pointer to a function used for heap
        allocations when more big integers are needed. This must be
        filled in when initialized.

    ReallocateMemory - Stores a pointer to a function used to
        reallocate memory. This must be filled in before the context
        is initialized.

    FreeMemory - Stores a pointer to a function used to free
        previously allocated memory. This must be filled in before
        the context is initialized.

    ActiveList - Stores a pointer to the outstanding big integers.

    FreeList - Stores a pointer to recently used but currently unused
        big integers.

    Radix - Stores a pointer to the radix used in the computation.

    Modulus - Stores the modulus used in the computation.

    Mu - Stores the mu values used in Barrett reduction.

    NormalizedMod - Stores the normalized modulo values.

    ExponentTable - Stores an array of pointers to integers representing
        pre-computed exponentiations of the working value.

    WindowSize - Stores the size of the sliding window.

    ActiveCount - Stores the number of integers on the active list.

    FreeCount - Stores the number of integers on the free list.

    ModOffset - Stores the modulo offset in use.

--*/

typedef struct _BIG_INTEGER_CONTEXT {
    PCY_ALLOCATE_MEMORY AllocateMemory;
    PCY_REALLOCATE_MEMORY ReallocateMemory;
    PCY_FREE_MEMORY FreeMemory;
    PBIG_INTEGER ActiveList;
    PBIG_INTEGER FreeList;
    PBIG_INTEGER Radix;
    PBIG_INTEGER Modulus[BIG_INTEGER_MODULO_COUNT];
    PBIG_INTEGER Mu[BIG_INTEGER_MODULO_COUNT];
    PBIG_INTEGER NormalizedMod[BIG_INTEGER_MODULO_COUNT];
    PBIG_INTEGER *ExponentTable;
    ULONG WindowSize;
    INTN ActiveCount;
    INTN FreeCount;
    UCHAR ModOffset;
} BIG_INTEGER_CONTEXT, *PBIG_INTEGER_CONTEXT;

typedef
PVOID
(*PCY_FILL_RANDOM) (
    PVOID Buffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine is called when the crypto library needs to fill a buffer with
    random bytes.

Arguments:

    Buffer - Supplies a pointer to the buffer to fill with random bytes.

    Size - Supplies the number of bytes of random data to return.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores the context used during encryption or decryption via
    RSA.

Members:

    BigIntegerContext - Stores the big integer context used to manage the
        values used during computation. It is expected that when the context
        is initialized the caller will have filled in the allocate, reallocate,
        and free memory functions in this structure.

    FillRandom - Stores a pointer to a function called to fill a buffer with
        random bytes. This function pointer must be filled in to do
        encryption with padding.

    Modulus - Stores the public modulus, p * q.

    PublicExponent - Stores the public exponent e.

    PrivateExponent - Stores the private exponent d.

    PValue - Stores one of the primes, p.

    QValue - Stores one the other prime, q.

    DpValue - Stores d mod (p - 1).

    DqValue - Stores d mod (q - 1).

    QInverse - Stores q^-1 mod p.

    ModulusSize - Stores the size of the modulus, in bytes.

--*/

typedef struct _RSA_CONTEXT {
    BIG_INTEGER_CONTEXT BigIntegerContext;
    PCY_FILL_RANDOM FillRandom;
    PBIG_INTEGER Modulus;
    PBIG_INTEGER PublicExponent;
    PBIG_INTEGER PrivateExponent;
    PBIG_INTEGER PValue;
    PBIG_INTEGER QValue;
    PBIG_INTEGER DpValue;
    PBIG_INTEGER DqValue;
    PBIG_INTEGER QInverse;
    UINTN ModulusSize;
} RSA_CONTEXT, *PRSA_CONTEXT;

/*++

Structure Description:

    This structure stores the raw values needed for a public key transfer.

Members:

    Modulus - Stores a pointer to the modulus value, the product of the two
        primes.

    ModulusLength - Stores the length of the modulus value in bytes.

    PublicExponent - Stores a pointer to the public key exponent.

    PublicExponentLength - Stores the length of the the public key exponent in
        bytes.

--*/

typedef struct _RSA_PUBLIC_KEY_COMPONENTS {
    PVOID Modulus;
    UINTN ModulusLength;
    PVOID PublicExponent;
    UINTN PublicExponentLength;
} RSA_PUBLIC_KEY_COMPONENTS, *PRSA_PUBLIC_KEY_COMPONENTS;

/*++

Structure Description:

    This structure stores the raw values needed for a private key transfer.

Members:

    PublicKey - Stores the public key components.

    PrivateExponent - Stores a pointer to the private key exponent.

    PrivateExponentLength - Stores the length of the private key exponent in
        bytes.

    PValue - Stores a pointer to one of the primes.

    PValueLength - Stores the length of the p value in bytes.

    QValue - Stores a pointer to the other prime.

    QValueLength - Stores the length of the q value in bytes.

    DpValue - Stores a pointer to the value d mod (p - 1).

    DpValueLength - Stores the length of the dP value in bytes.

    DqValue - Stores a pointer to the value d mod (q - 1).

    DqValueLength - Stores the length of the dQ value in bytes.

    QInverse - Stores a pointer to the value q^-1 mod p.

    QInverseLength - Stores the length of the q inverse value in bytes.

--*/

typedef struct _RSA_PRIVATE_KEY_COMPONENTS {
    RSA_PUBLIC_KEY_COMPONENTS PublicKey;
    PVOID PrivateExponent;
    UINTN PrivateExponentLength;
    PVOID PValue;
    UINTN PValueLength;
    PVOID QValue;
    UINTN QValueLength;
    PVOID DpValue;
    UINTN DpValueLength;
    PVOID DqValue;
    UINTN DqValueLength;
    PVOID QInverse;
    UINTN QInverseLength;
} RSA_PRIVATE_KEY_COMPONENTS, *PRSA_PRIVATE_KEY_COMPONENTS;

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

    Ciphertext - Supplies a pointer to the ciphertext buffer.

    Plaintext - Supplies a pointer where the plaintext will be returned.

    Length - Supplies the length of the plaintext and ciphertext buffers, in
        bytes. This length must be a multiple of 16 bytes.

Return Value:

    None.

--*/

CRYPTO_API
VOID
CyAesEcbEncrypt (
    PAES_CONTEXT Context,
    PUCHAR Plaintext,
    PUCHAR Ciphertext,
    INT Length
    );

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

CRYPTO_API
VOID
CyAesEcbDecrypt (
    PAES_CONTEXT Context,
    PUCHAR Ciphertext,
    PUCHAR Plaintext,
    INT Length
    );

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

CRYPTO_API
VOID
CyAesCtrEncrypt (
    PAES_CONTEXT Context,
    PUCHAR Plaintext,
    PUCHAR Ciphertext,
    INT Length
    );

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

CRYPTO_API
VOID
CyAesCtrDecrypt (
    PAES_CONTEXT Context,
    PUCHAR Ciphertext,
    PUCHAR Plaintext,
    INT Length
    );

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

CRYPTO_API
KSTATUS
CyRsaInitializeContext (
    PRSA_CONTEXT Context
    );

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

CRYPTO_API
VOID
CyRsaDestroyContext (
    PRSA_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys a previously intialized RSA context.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

CRYPTO_API
KSTATUS
CyRsaLoadPrivateKey (
    PRSA_CONTEXT Context,
    PRSA_PRIVATE_KEY_COMPONENTS PrivateKey
    );

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

CRYPTO_API
KSTATUS
CyRsaLoadPublicKey (
    PRSA_CONTEXT Context,
    PRSA_PUBLIC_KEY_COMPONENTS PublicKey
    );

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

CRYPTO_API
INTN
CyRsaDecrypt (
    PRSA_CONTEXT Context,
    PVOID Ciphertext,
    PVOID Plaintext,
    BOOL IsDecryption
    );

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

CRYPTO_API
INTN
CyRsaEncrypt (
    PRSA_CONTEXT Context,
    PVOID Plaintext,
    UINTN PlaintextLength,
    PVOID Ciphertext,
    BOOL IsSigning
    );

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

CRYPTO_API
KSTATUS
CyRsaAddPemFile (
    PRSA_CONTEXT RsaContext,
    PVOID PemFile,
    UINTN PemFileLength,
    PSTR Password
    );

/*++

Routine Description:

    This routine attempts to add a private key to the given RSA context.

Arguments:

    RsaContext - Supplies a pointer to the previously initialized RSA context.

    PemFile - Supplies a pointer to the PEM file contents.

    PemFileLength - Supplies the length of the PEM file contents.

    Password - Supplies an optional pointer to a password to decrypt the
        private key if needed.

Return Value:

    Status code.

--*/

CRYPTO_API
UINTN
CyBase64GetDecodedLength (
    UINTN EncodedDataLength
    );

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

CRYPTO_API
UINTN
CyBase64GetEncodedLength (
    UINTN DataLength
    );

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

CRYPTO_API
BOOL
CyBase64Decode (
    PSTR EncodedData,
    UINTN EncodedDataLength,
    PUCHAR Data,
    PUINTN DataLength
    );

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

CRYPTO_API
VOID
CyBase64Encode (
    PUCHAR Data,
    UINTN DataLength,
    PSTR EncodedData,
    PUINTN EncodedDataLength
    );

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

//
// -------------------------------------------------------- Function Prototypes
//


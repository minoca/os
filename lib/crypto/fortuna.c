/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fortuna.c

Abstract:

    This module implements support for the Fortuna Pseudo-Random Number
    Generator.

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

#define FORTUNA_POOL0_FILL 32
#define FORTUNA_RESEED_SIZE (1024 * 1024)
#define FORTUNA_RESEED_INTERVAL_MILLISECONDS 100

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CypFortunaSpreadInitialSeed (
    PFORTUNA_CONTEXT Context
    );

VOID
CypFortunaRekey (
    PFORTUNA_CONTEXT Context
    );

BOOL
CypFortunaCheckReseedTime (
    PFORTUNA_CONTEXT Context
    );

VOID
CypFortunaReseed (
    PFORTUNA_CONTEXT Context
    );

VOID
CypFortunaEncryptCounter (
    PFORTUNA_CONTEXT Context,
    UCHAR Buffer[FORTUNA_BLOCK_SIZE]
    );

VOID
CypFortunaIncrementCounter (
    PFORTUNA_CONTEXT Context
    );

UINTN
CypFortunaGetRandomPoolIndex (
    PFORTUNA_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CRYPTO_API
VOID
CyFortunaInitialize (
    PFORTUNA_CONTEXT Context,
    PCY_GET_TIME_COUNTER GetTimeCounterFunction,
    ULONGLONG TimeCounterFrequency
    )

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

{

    UINTN Pool;

    RtlZeroMemory(Context, sizeof(FORTUNA_CONTEXT));
    for (Pool = 0; Pool < FORTUNA_POOL_COUNT; Pool += 1) {
        CySha256Initialize(&(Context->Pools[Pool]));
    }

    Context->GetTimeCounter = GetTimeCounterFunction;
    Context->TimeCounterFrequency = TimeCounterFrequency;
    return;
}

CRYPTO_API
VOID
CyFortunaGetRandomBytes (
    PFORTUNA_CONTEXT Context,
    PUCHAR Data,
    UINTN Size
    )

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

{

    UINTN BlockNumber;
    UINTN CopySize;

    BlockNumber = 0;

    ASSERT(Context->Initialized != FortunaNotInitialized);

    //
    // Spread the seed around a bit if this is the first time.
    //

    if (Context->Initialized < FortunaInitialized) {
        CypFortunaSpreadInitialSeed(Context);
        Context->Initialized = FortunaInitialized;
    }

    //
    // Determine if a reseed should occur.
    //

    if ((Context->Pool0Bytes >= FORTUNA_POOL0_FILL) ||
        (Context->ReseedCount == 0)) {

        if (CypFortunaCheckReseedTime(Context) != FALSE) {
            CypFortunaReseed(Context);
        }
    }

    while (Size > 0) {
        CypFortunaEncryptCounter(Context, Context->Result);
        CopySize = Size;
        if (CopySize > FORTUNA_BLOCK_SIZE) {
            CopySize = FORTUNA_BLOCK_SIZE;
        }

        RtlCopyMemory(Data, Context->Result, CopySize);
        Data += CopySize;
        Size -= CopySize;

        //
        // Avoid giving out too many bytes from a single key.
        //

        BlockNumber += 1;
        if (BlockNumber > (FORTUNA_RESEED_SIZE / FORTUNA_BLOCK_SIZE)) {
            CypFortunaRekey(Context);
            BlockNumber = 0;
        }
    }

    //
    // Re-key for the next request.
    //

    CypFortunaRekey(Context);
    return;
}

CRYPTO_API
VOID
CyFortunaAddEntropy (
    PFORTUNA_CONTEXT Context,
    PVOID Data,
    UINTN Size
    )

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

{

    UCHAR Hash[SHA256_HASH_SIZE];
    SHA256_CONTEXT HashContext;
    UINTN PoolIndex;

    //
    // Hash the data handed in.
    //

    CySha256Initialize(&HashContext);
    CySha256AddContent(&HashContext, Data, Size);
    CySha256GetHash(&HashContext, Hash);

    //
    // Make sure pool zero is initialized, otherwise update randomly.
    //

    if (Context->ReseedCount == 0) {
        PoolIndex = 0;
        if (Context->Initialized == FortunaNotInitialized) {
            Context->Initialized = FortunaInitializationSeeded;
        }

    } else {
        PoolIndex = CypFortunaGetRandomPoolIndex(Context);
    }

    CySha256AddContent(&(Context->Pools[PoolIndex]), Hash, SHA256_HASH_SIZE);
    if (PoolIndex == 0) {
        Context->Pool0Bytes += SHA256_HASH_SIZE;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CypFortunaSpreadInitialSeed (
    PFORTUNA_CONTEXT Context
    )

/*++

Routine Description:

    This routine spreads the entropy gained so far around the pools.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    UCHAR Buffer[FORTUNA_HASH_KEY_SIZE];
    UINTN PoolIndex;

    //
    // Use the next block as the initial counter value.
    //

    CypFortunaEncryptCounter(Context, Context->Counter);

    //
    // Shuffle all pools except pool zero.
    //

    for (PoolIndex = 0; PoolIndex < FORTUNA_POOL_COUNT; PoolIndex += 1) {
        CypFortunaEncryptCounter(Context, Buffer);
        CypFortunaEncryptCounter(Context, Buffer + FORTUNA_BLOCK_SIZE);
        CySha256AddContent(&(Context->Pools[PoolIndex]),
                           Buffer,
                           FORTUNA_HASH_KEY_SIZE);
    }

    RtlZeroMemory(Buffer, sizeof(Buffer));

    //
    // Hide the key.
    //

    CypFortunaRekey(Context);
    return;
}

VOID
CypFortunaRekey (
    PFORTUNA_CONTEXT Context
    )

/*++

Routine Description:

    This routine selects a different cipher key for use in future block
    generation.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    //
    // Use the two next blocks as the new key.
    //

    CypFortunaEncryptCounter(Context, Context->Key);
    CypFortunaEncryptCounter(Context, Context->Key + FORTUNA_BLOCK_SIZE);
    CyAesInitialize(&(Context->CipherContext),
                    AesModeCbc256,
                    Context->Key,
                    NULL);

    return;
}

BOOL
CypFortunaCheckReseedTime (
    PFORTUNA_CONTEXT Context
    )

/*++

Routine Description:

    This routine determines whether it is time for the context to be reseeded
    or not.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    TRUE if enough time has passed such that the context can be reseeded.

    FALSE if it hasn't been long enough since the last reseed.

--*/

{

    ULONGLONG CurrentTime;
    ULONGLONG DeltaTicks;
    ULONGLONG Milliseconds;

    if ((Context->GetTimeCounter == NULL) ||
        (Context->TimeCounterFrequency == 0)) {

        return TRUE;
    }

    CurrentTime = Context->GetTimeCounter();
    DeltaTicks = CurrentTime - Context->LastReseedTime;

    //
    // Compute the number of milliseconds since the last update.
    //

    Milliseconds = (DeltaTicks * 1000ULL) / Context->TimeCounterFrequency;
    if (Milliseconds >= FORTUNA_RESEED_INTERVAL_MILLISECONDS) {
        Context->LastReseedTime = CurrentTime;
        return TRUE;
    }

    return FALSE;
}

VOID
CypFortunaReseed (
    PFORTUNA_CONTEXT Context
    )

/*++

Routine Description:

    This routine selects a completely new cipher key using the entropy pools.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    UCHAR Buffer[FORTUNA_HASH_KEY_SIZE];
    SHA256_CONTEXT KeyHashContext;
    UINTN PoolIndex;
    UINTN ReseedCount;

    //
    // Mark the pool as empty.
    //

    Context->Pool0Bytes = 0;
    Context->ReseedCount += 1;
    ReseedCount = Context->ReseedCount;

    //
    // The 0th pool is used every 2nd time, the 1st pool is used every 4th
    // time, the 2nd pool is used every 8th time, etc. Pool i is used every
    // 2^ith time.
    //

    CySha256Initialize(&KeyHashContext);
    for (PoolIndex = 0; PoolIndex < FORTUNA_POOL_COUNT; PoolIndex += 1) {
        CySha256GetHash(&(Context->Pools[PoolIndex]), Buffer);
        CySha256AddContent(&KeyHashContext, Buffer, FORTUNA_HASH_KEY_SIZE);
        if (((ReseedCount & 0x1) != 0) || (ReseedCount == 0)) {
            break;
        }

        ReseedCount >>= 1;
    }

    //
    // Add the old key into the mix too.
    //

    CySha256AddContent(&KeyHashContext, Context->Key, FORTUNA_HASH_KEY_SIZE);

    //
    // Get the new key.
    //

    CySha256GetHash(&KeyHashContext, Context->Key);

    //
    // Use the new key in future cipher blocks.
    //

    CyAesInitialize(&(Context->CipherContext),
                    AesModeCbc256,
                    Context->Key,
                    NULL);

    //
    // Avoid leaking state onto the stack.
    //

    RtlZeroMemory(Buffer, sizeof(Buffer));
    RtlZeroMemory(&KeyHashContext, sizeof(SHA256_CONTEXT));
    return;
}

VOID
CypFortunaEncryptCounter (
    PFORTUNA_CONTEXT Context,
    UCHAR Buffer[FORTUNA_BLOCK_SIZE]
    )

/*++

Routine Description:

    This routine simply encrypts a new counter value.

Arguments:

    Context - Supplies a pointer to the context.

    Buffer - Supplies a pointer where the new block will be returned.

Return Value:

    None.

--*/

{

    CyAesCbcEncrypt(&(Context->CipherContext),
                    Context->Counter,
                    Buffer,
                    FORTUNA_BLOCK_SIZE);

    CypFortunaIncrementCounter(Context);
    return;
}

VOID
CypFortunaIncrementCounter (
    PFORTUNA_CONTEXT Context
    )

/*++

Routine Description:

    This routine simply increments the current counter value.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    None.

--*/

{

    PUINTN Value;

    Value = (PUINTN)(Context->Counter);
    Value[0] += 1;
    if (Value[0] != 0) {
        return;
    }

    Value[1] += 1;
    if (Value[1] != 0) {
        return;
    }

    Value[2] += 1;
    if (Value[2] != 0) {
        return;
    }

    Value[3] += 1;
    return;
}

UINTN
CypFortunaGetRandomPoolIndex (
    PFORTUNA_CONTEXT Context
    )

/*++

Routine Description:

    This routine returns a pool index to feed entropy into.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    Returns a pool index.

--*/

{

    UINTN Index;

    Index = Context->Key[Context->Position] % FORTUNA_POOL_COUNT;
    Context->Position += 1;
    if (Context->Position >= FORTUNA_HASH_KEY_SIZE) {
        Context->Position = 0;
    }

    return Index;
}


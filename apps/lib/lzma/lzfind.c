/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzfind.c

Abstract:

    This module implements support for finding matches when encoding an LZMA
    stream.

Author:

    Evan Green 27-Oct-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <string.h>

#include <minoca/lib/types.h>
#include <minoca/lib/lzma.h>
#include "lzmap.h"
#include "lzfind.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro moves the match finder position forward by one.
//

#define LZP_MATCH_FINDER_ADVANCE(_Finder)                   \
    (_Finder)->CyclicBufferPosition += 1;                   \
    (_Finder)->Buffer += 1;                                 \
    (_Finder)->Position += 1;                               \
    if ((_Finder)->Position == (_Finder)->PositionLimit) {  \
        LzpMatchFinderCheckLimits((_Finder)); \
    }

//
// This macro reduces the match finder offsets by the given amount.
//

#define LZP_MATCH_FINDER_REDUCE_OFFSETS(_Finder, _Value)    \
    (_Finder)->PositionLimit -= (_Value);                   \
    (_Finder)->Position -= (_Value);                        \
    (_Finder)->StreamPosition -= (_Value);

//
// This macro determines whether the match finder needs to be moved.
//

#define LZP_MATCH_FINDER_NEEDS_MOVE(_Finder) \
    (((_Finder)->DirectInput == FALSE) && \
     ((UINTN)((_Finder)->BufferBase + (_Finder)->BlockSize - \
              (_Finder)->Buffer) <= (_Finder)->KeepSizeAfter))

//
// This macro computes the hash value for the given 2 bytes.
//

#define LZP_HASH2(_Finder, _Bytes) \
    ((_Bytes)[0] | ((ULONG)(_Bytes)[1] << 8))

//
// This macro computes the hash values for the given 2 and 3 bytes.
//

#define LZP_HASH3(_Finder, _Bytes, _HashValue2, _HashValue3) \
    (_HashValue3) = LzCrc32[(_Bytes)[0]] ^ (_Bytes)[1]; \
    (_HashValue2) = (_HashValue3) & (LZMA_HASH2_SIZE - 1); \
    (_HashValue3) = ((_HashValue3) ^ ((ULONG)(_Bytes)[2] << 8)) & \
                    (_Finder)->HashMask;

//
// This macro computes the hash values for the given 2, 3, and 4 bytes.
//

#define LZP_HASH4(_Finder, _Bytes, _HashValue2, _HashValue3, _HashValue4) \
    (_HashValue4) = LzCrc32[(_Bytes)[0]] ^ (_Bytes)[1]; \
    (_HashValue2) = (_HashValue4) & (LZMA_HASH2_SIZE - 1); \
    (_HashValue4) ^= ((ULONG)(_Bytes)[2]) << 8; \
    (_HashValue3) = (_HashValue4) & (LZMA_HASH3_SIZE - 1); \
    (_HashValue4) = ((_HashValue4) ^ (LzCrc32[(_Bytes)[3]] << 5)) & \
                    (_Finder)->HashMask;

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_HASH2_SIZE (1 << 10)
#define LZMA_HASH3_SIZE (1 << 16)
#define LZMA_HASH4_SIZE (1 << 20)

//
// Define the offsets into the giant flattened array for the larger hash tables.
//

#define LZMA_HASH3_OFFSET LZMA_HASH2_SIZE
#define LZMA_HASH4_OFFSET (LZMA_HASH2_SIZE + LZMA_HASH3_SIZE)

#define LZMA_EMPTY_HASH 0
#define LZMA_MAX_VALUE_FOR_NORMALIZE ((ULONG)0xFFFFFFFF)
#define LZMA_NORMALIZE_STEP_MIN (1 << 10)
#define LZMA_NORMALIZE_MASK (~(ULONG)(LZMA_NORMALIZE_STEP_MIN - 1))

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
LzpMatchFinderInitialize (
    PLZ_MATCH_FINDER Finder
    );

LZ_STATUS
LzpMatchFinderLoad (
    PLZ_MATCH_FINDER Finder,
    LZ_FLUSH_OPTION Flush
    );

ULONG
LzpMatchFinderGetCount (
    PLZ_MATCH_FINDER Finder
    );

PCUCHAR
LzpMatchFinderGetPosition (
    PLZ_MATCH_FINDER Finder
    );

ULONG
LzpMatchFinderHash4GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    );

VOID
LzpMatchFinderHash4Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    );

ULONG
LzpMatchFinderBinTree2GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    );

VOID
LzpMatchFinderBinTree2Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    );

ULONG
LzpMatchFinderBinTree3GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    );

VOID
LzpMatchFinderBinTree3Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    );

ULONG
LzpMatchFinderBinTree4GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    );

VOID
LzpMatchFinderBinTree4Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    );

VOID
LzpMatchFinderSetDefaults (
    PLZ_MATCH_FINDER Finder
    );

VOID
LzpMatchFinderInitializeContext (
    PLZ_MATCH_FINDER Finder,
    BOOL ReadData
    );

VOID
LzpMatchFinderReadBlock (
    PLZ_MATCH_FINDER Finder
    );

PULONG
LzpMatchFinderHashGetMatches (
    PLZ_MATCH_FINDER Finder,
    ULONG LengthLimit,
    ULONG CurrentMatch,
    PULONG Distances,
    ULONG MaxLength
    );

PULONG
LzpMatchFinderTreeGetMatches (
    PLZ_MATCH_FINDER Finder,
    ULONG LengthLimit,
    ULONG CurrentMatch,
    PULONG Distances,
    ULONG MaxLength
    );

VOID
LzpMatchFinderTreeSkipMatches (
    PLZ_MATCH_FINDER Finder,
    ULONG LengthLimit,
    ULONG CurrentMatch
    );

VOID
LzpMatchFinderCheckLimits (
    PLZ_MATCH_FINDER Finder
    );

VOID
LzpMatchFinderSetLimits (
    PLZ_MATCH_FINDER Finder
    );

VOID
LzpMatchFinderNormalize (
    PLZ_MATCH_FINDER Finder
    );

VOID
LzpMatchFinderNormalizeTable (
    ULONG Value,
    PLZ_REFERENCE Table,
    UINTN Count
    );

VOID
LzpMatchFinderMoveBlock (
    PLZ_MATCH_FINDER Finder
    );

//
// -------------------------------------------------------------------- Globals
//

const LZ_MATCH_FINDER_INTERFACE LzDefaultMatchFinderInterface = {
    LzpMatchFinderInitialize,
    LzpMatchFinderLoad,
    LzpMatchFinderGetCount,
    LzpMatchFinderGetPosition,
    LzpMatchFinderBinTree4GetMatches,
    LzpMatchFinderBinTree4Skip
};

//
// ------------------------------------------------------------------ Functions
//

VOID
LzpInitializeMatchFinder (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine initializes an LZMA match finder.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    Finder->BufferBase = NULL;
    Finder->DirectInput = FALSE;
    Finder->Hash = NULL;
    LzpMatchFinderSetDefaults(Finder);
    return;
}

VOID
LzpDestroyMatchFinder (
    PLZ_MATCH_FINDER Finder,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys an LZMA match finder.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Context - Supplies a pointer to the system context.

Return Value:

    None.

--*/

{

    Context->Reallocate(Finder->Hash, 0);
    if (Finder->DirectInput == FALSE) {
        Context->Reallocate(Finder->BufferBase, 0);
        Finder->BufferBase = NULL;
    }

    return;
}

LZ_STATUS
LzpMatchFinderAllocateBuffers (
    PLZ_MATCH_FINDER Finder,
    ULONG HistorySize,
    ULONG KeepAddBufferBefore,
    ULONG MatchMaxLength,
    ULONG KeepAddBufferAfter,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine allocates the match finder buffers now that all parameters
    have been determined.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    HistorySize - Supplies the size of the history to keep.

    KeepAddBufferBefore - Supplies the additional history size to keep before
        the window.

    MatchMaxLength - Supplies the maximum valid length of a match.

    KeepAddBufferAfter - Supplies the additional history size to keep after
        the window.

    Context - Supplies the system context.

Return Value:

    LZ Status.

--*/

{

    UINTN AllocationSize;
    ULONG BlockSize;
    ULONG HashMask;
    ULONG HashSum;
    ULONG NewCyclicBufferSize;
    UINTN NewSize;
    ULONG Reserve;
    UINTN SonCount;

    if (HistorySize > LZMA_MAX_HISTORY_SIZE) {
        LzpDestroyMatchFinder(Finder, Context);
        return LzErrorInvalidParameter;
    }

    Reserve = HistorySize >> 1;
    if (HistorySize >= (3L << 30)) {
        Reserve = HistorySize >> 3;

    } else if (HistorySize >= (2L << 30)) {
        Reserve = HistorySize >> 2;
    }

    Reserve += (KeepAddBufferBefore + MatchMaxLength + KeepAddBufferAfter) / 2;
    Reserve += 1 << 19;
    Finder->KeepSizeBefore = HistorySize + KeepAddBufferBefore + 1;
    Finder->KeepSizeAfter = MatchMaxLength + KeepAddBufferAfter;

    //
    // Allocate the window.
    //

    BlockSize = Finder->KeepSizeBefore + Finder->KeepSizeAfter + Reserve;
    if (Finder->DirectInput != FALSE) {
        Finder->BlockSize = BlockSize;

    } else {
        if ((Finder->BufferBase == NULL) || (Finder->BlockSize != BlockSize)) {
            if (Finder->BufferBase != NULL) {
                Context->Reallocate(Finder->BufferBase, 0);
            }

            Finder->BlockSize = BlockSize;
            Finder->BufferBase = Context->Reallocate(NULL, BlockSize);
            if (Finder->BufferBase == NULL) {
                LzpDestroyMatchFinder(Finder, Context);
                return LzErrorMemory;
            }
        }
    }

    NewCyclicBufferSize = HistorySize + 1;
    Finder->MatchMaxLength = MatchMaxLength;
    Finder->FixedHashSize = 0;
    if (Finder->HashByteCount == 2) {
        HashMask = (1 << 16) - 1;

    } else {
        HashMask = HistorySize - 1;
        HashMask |= (HashMask >> 1);
        HashMask |= (HashMask >> 2);
        HashMask |= (HashMask >> 4);
        HashMask |= (HashMask >> 8);
        HashMask >>= 1;
        HashMask |= 0xFFFF;
        if (HashMask > (1 << 24)) {
            if (Finder->HashByteCount == 3) {
                HashMask = (1 << 24) - 1;

            } else {
                HashMask >>= 1;
            }
        }
    }

    Finder->HashMask = HashMask;
    HashSum = HashMask + 1;
    if (Finder->HashByteCount > 2) {
        Finder->FixedHashSize += LZMA_HASH2_SIZE;
    }

    if (Finder->HashByteCount > 3) {
        Finder->FixedHashSize += LZMA_HASH3_SIZE;
    }

    if (Finder->HashByteCount > 4) {
        Finder->FixedHashSize += LZMA_HASH4_SIZE;
    }

    HashSum += Finder->FixedHashSize;
    Finder->HistorySize = HistorySize;
    Finder->HashSizeSum = HashSum;
    Finder->CyclicBufferSize = NewCyclicBufferSize;
    SonCount = NewCyclicBufferSize;
    if (Finder->BinTreeMode != FALSE) {
        SonCount <<= 1;
    }

    NewSize = HashSum + SonCount;
    if ((Finder->Hash == NULL) || (Finder->ReferenceCount != NewSize)) {
        if (Finder->Hash != NULL) {
            Context->Reallocate(Finder->Hash, 0);
            Finder->Hash = NULL;
        }

        AllocationSize = NewSize * sizeof(LZ_REFERENCE);
        if (AllocationSize / sizeof(LZ_REFERENCE) != NewSize) {
            return LzErrorMemory;
        }

        Finder->ReferenceCount = NewSize;
        Finder->Hash = Context->Reallocate(NULL, AllocationSize);
        if (Finder->Hash == NULL) {
            return LzErrorMemory;
        }

        Finder->Son = Finder->Hash + Finder->HashSizeSum;
    }

    return LzSuccess;
}

VOID
LzpMatchFinderInitializeInterface (
    PLZ_MATCH_FINDER Finder,
    PLZ_MATCH_FINDER_INTERFACE Interface
    )

/*++

Routine Description:

    This routine initializes the match finder function table.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Interface - Supplies a pointer where the populated function pointers will
        be returned.

Return Value:

    None.

--*/

{

    //
    // Copy the default, which is the BinTree size 4 methods.
    //

    memcpy(Interface,
           &LzDefaultMatchFinderInterface,
           sizeof(LZ_MATCH_FINDER_INTERFACE));

    if (Finder->BinTreeMode == FALSE) {
        Interface->GetMatches = LzpMatchFinderHash4GetMatches;
        Interface->Skip = LzpMatchFinderHash4Skip;

    } else {
        if (Finder->HashByteCount == 2) {
            Interface->GetMatches = LzpMatchFinderBinTree2GetMatches;
            Interface->Skip = LzpMatchFinderBinTree2Skip;

        } else if (Finder->HashByteCount == 3) {
            Interface->GetMatches = LzpMatchFinderBinTree3GetMatches;
            Interface->Skip = LzpMatchFinderBinTree3Skip;
        }
    }

    return;
}

VOID
LzpMatchFinderInitialize (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine initializes a match finder context.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    LzpMatchFinderInitializeContext(Finder, TRUE);
    return;
}

LZ_STATUS
LzpMatchFinderLoad (
    PLZ_MATCH_FINDER Finder,
    LZ_FLUSH_OPTION Flush
    )

/*++

Routine Description:

    This routine loads data into a match finder context. It is primarily needed
    as a data shuttle in case the user supplied less than KeepSizeAfter bytes.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Flush - Supplies the flush option the encoder was called with, which
        indicates whether more input data is coming or not.

Return Value:

    LZ Status code.

    Returns LzErrorProgress if input data was read but it was not enough to
    process.

--*/

{

    LzpMatchFinderCheckLimits(Finder);

    //
    // If this is not the end of the stream but there's not enough data to get
    // the maximum match, then indicate not to encode just yet.
    //

    if ((Finder->StreamEndWasReached == FALSE) &&
        (Finder->StreamPosition - Finder->Position <= Finder->KeepSizeAfter)) {

        if (Flush == LzNoFlush) {
            return LzErrorProgress;
        }

        //
        // Either there's no more input or this is a complete flush. Either way
        // no more input is coming.
        //

        Finder->StreamEndWasReached = TRUE;
    }

    return Finder->Result;
}

ULONG
LzpMatchFinderGetCount (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine returns the number of available bytes in the finder context.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    Returns the number of available bytes.

--*/

{

    //
    // Return the buffer size, which is the position at the end of the buffer
    // minus the position at the beginning of the buffer.
    //

    return Finder->StreamPosition - Finder->Position;
}

PCUCHAR
LzpMatchFinderGetPosition (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine returns a pointer to the current position in the match finder
    context.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    Returns a pointer to the current position within the source.

--*/

{

    return Finder->Buffer;
}

ULONG
LzpMatchFinderHash4GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    )

/*++

Routine Description:

    This routine finds matches using a 4 byte hash table.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Distances - Supplies a pointer where the distances will be returned on
        success.

Return Value:

    Returns the number of matches times two.

--*/

{

    PCUCHAR Current;
    LZ_REFERENCE CurrentMatch;
    LZ_REFERENCE Distance2;
    LZ_REFERENCE Distance3;
    PULONG DistancesEnd;
    PLZ_REFERENCE Hash;
    ULONG HashValue;
    ULONG HashValue2;
    ULONG HashValue3;
    ULONG LengthLimit;
    PCUCHAR Limit;
    ULONG MaxLength;
    ULONG Offset;
    LZ_REFERENCE Position;
    PCUCHAR Search;

    LengthLimit = Finder->LengthLimit;
    if (LengthLimit < 4) {
        LZP_MATCH_FINDER_ADVANCE(Finder);
        return 0;
    }

    Current = Finder->Buffer;

    //
    // Perform the hash calculation, getting the 2 and 3 byte hashes along the
    // way.
    //

    LZP_HASH4(Finder, Current, HashValue2, HashValue3, HashValue);

    //
    // Get the previous distances for the last 2, 3, and 4 bytes.
    //

    Hash = Finder->Hash;
    Position = Finder->Position;
    Distance2 = Position - Hash[HashValue2];
    Distance3 = Position - Hash[LZMA_HASH3_OFFSET + HashValue3];
    CurrentMatch = Hash[LZMA_HASH4_OFFSET + HashValue];

    //
    // Save the new distances in the hash tables.
    //

    Hash[HashValue2] = Position;
    Hash[LZMA_HASH3_OFFSET + HashValue3] = Position;
    Hash[LZMA_HASH4_OFFSET + HashValue] = Position;

    //
    // Get the longest match length.
    //

    MaxLength = 0;
    Offset = 0;
    if ((Distance2 < Finder->CyclicBufferSize) &&
        (*(Current - Distance2) == *Current)) {

        MaxLength = 2;
        Distances[0] = 2;
        Distances[1] = Distance2 - 1;
        Offset = 2;
    }

    if ((Distance3 != Distance2) &&
        (Distance3 < Finder->CyclicBufferSize) &&
        (*(Current - Distance3) == *Current)) {

        MaxLength = 3;
        Distances[Offset + 1] = Distance3 - 1;
        Offset += 2;
        Distance2 = Distance3;
    }

    if (Offset != 0) {

        //
        // See how long of a match it is.
        //

        Search = Current + MaxLength;
        Limit = Current + LengthLimit;
        while ((Search != Limit) && (*(Search - Distance2) == *Search)) {
            Search += 1;
        }

        MaxLength = Search - Current;
        Distances[Offset - 2] = MaxLength;
        if (MaxLength == LengthLimit) {
            Finder->Son[Finder->CyclicBufferPosition] = CurrentMatch;
            LZP_MATCH_FINDER_ADVANCE(Finder);
            return Offset;
        }
    }

    if (MaxLength < 3) {
        MaxLength = 3;
    }

    DistancesEnd = LzpMatchFinderHashGetMatches(Finder,
                                                LengthLimit,
                                                CurrentMatch,
                                                Distances + Offset,
                                                MaxLength);

    Offset = DistancesEnd - Distances;
    LZP_MATCH_FINDER_ADVANCE(Finder);
    return Offset;
}

VOID
LzpMatchFinderHash4Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    )

/*++

Routine Description:

    This routine skips a portion of the input.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Size - Supplies the number of bytes to skip.

Return Value:

    None.

--*/

{

    PCUCHAR Current;
    ULONG CurrentMatch;
    PLZ_REFERENCE Hash;
    ULONG HashValue;
    ULONG HashValue2;
    ULONG HashValue3;
    ULONG LengthLimit;

    do {

        //
        // Skip the header.
        //

        LengthLimit = Finder->LengthLimit;
        if (LengthLimit < 4) {
            LZP_MATCH_FINDER_ADVANCE(Finder);
            Size -= 1;
            continue;
        }

        Current = Finder->Buffer;

        //
        // Perform the hash calculation, getting the 2 and 3 byte hashes along
        // the way.
        //

        LZP_HASH4(Finder, Current, HashValue2, HashValue3, HashValue);

        //
        // Set the hash table values for the 2, 3, and 4 byte hash tables to
        // the current position.
        //

        Hash = Finder->Hash;
        CurrentMatch = Hash[LZMA_HASH4_OFFSET + HashValue];
        Hash[HashValue2] = Finder->Position;
        Hash[LZMA_HASH3_OFFSET + HashValue3] = Finder->Position;
        Hash[LZMA_HASH4_OFFSET + HashValue] = Finder->Position;
        Finder->Son[Finder->CyclicBufferPosition] = CurrentMatch;
        LZP_MATCH_FINDER_ADVANCE(Finder);
        Size -= 1;

    } while (Size != 0);

    return;
}

ULONG
LzpMatchFinderBinTree2GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    )

/*++

Routine Description:

    This routine finds matches using the 2 byte binary tree.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Distances - Supplies a pointer where the distances will be returned on
        success.

Return Value:

    Returns the number of matches times two.

--*/

{

    PCUCHAR Current;
    LZ_REFERENCE CurrentMatch;
    PULONG DistancesEnd;
    ULONG HashValue;
    ULONG LengthLimit;
    ULONG Offset;

    LengthLimit = Finder->LengthLimit;
    if (LengthLimit < 2) {
        LZP_MATCH_FINDER_ADVANCE(Finder);
        return 0;
    }

    Current = Finder->Buffer;
    HashValue = LZP_HASH2(Finder, Current);
    CurrentMatch = Finder->Hash[HashValue];
    Finder->Hash[HashValue] = Finder->Position;
    Offset = 0;
    DistancesEnd = LzpMatchFinderTreeGetMatches(Finder,
                                                LengthLimit,
                                                CurrentMatch,
                                                Distances + Offset,
                                                1);

    Offset = DistancesEnd - Distances;
    LZP_MATCH_FINDER_ADVANCE(Finder);
    return Offset;
}

VOID
LzpMatchFinderBinTree2Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    )

/*++

Routine Description:

    This routine skips a portion of the input when using the 2 byte binary tree.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Size - Supplies the number of bytes to skip.

Return Value:

    None.

--*/

{

    PCUCHAR Current;
    ULONG CurrentMatch;
    ULONG HashValue;
    ULONG LengthLimit;

    do {

        //
        // Skip the header.
        //

        LengthLimit = Finder->LengthLimit;
        if (LengthLimit < 2) {
            LZP_MATCH_FINDER_ADVANCE(Finder);
            Size -= 1;
            continue;
        }

        Current = Finder->Buffer;
        HashValue = LZP_HASH2(Finder, Current);
        CurrentMatch = Finder->Hash[HashValue];
        Finder->Hash[HashValue] = Finder->Position;
        LzpMatchFinderTreeSkipMatches(Finder, LengthLimit, CurrentMatch);
        LZP_MATCH_FINDER_ADVANCE(Finder);
        Size -= 1;

    } while (Size != 0);

    return;
}

ULONG
LzpMatchFinderBinTree3GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    )

/*++

Routine Description:

    This routine finds matches using the 3 byte binary tree.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Distances - Supplies a pointer where the distances will be returned on
        success.

Return Value:

    Returns the number of matches times two.

--*/

{

    PCUCHAR Current;
    LZ_REFERENCE CurrentMatch;
    ULONG Distance2;
    PULONG DistancesEnd;
    PLZ_REFERENCE Hash;
    ULONG HashValue;
    ULONG HashValue2;
    ULONG LengthLimit;
    PCUCHAR Limit;
    ULONG MaxLength;
    ULONG Offset;
    ULONG Position;
    PCUCHAR Search;

    LengthLimit = Finder->LengthLimit;
    if (LengthLimit < 3) {
        LZP_MATCH_FINDER_ADVANCE(Finder);
        return 0;
    }

    Current = Finder->Buffer;
    LZP_HASH3(Finder, Current, HashValue2, HashValue);
    Hash = Finder->Hash;
    Position = Finder->Position;
    Distance2 = Position - Hash[HashValue2];
    CurrentMatch = Hash[LZMA_HASH3_OFFSET + HashValue];
    Hash[HashValue2] = Position;
    Hash[LZMA_HASH3_OFFSET + HashValue] = Position;
    MaxLength = 2;
    Offset = 0;
    if ((Distance2 < Finder->CyclicBufferSize) &&
        (*(Current - Distance2) == *Current)) {

        //
        // See how long of a match it is.
        //

        Search = Current + MaxLength;
        Limit = Current + LengthLimit;
        while ((Search != Limit) && (*(Search - Distance2) == *Search)) {
            Search += 1;
        }

        MaxLength = Search - Current;
        Distances[0] = MaxLength;
        Distances[1] = Distance2 - 1;
        Offset = 2;
        if (MaxLength == LengthLimit) {
            LzpMatchFinderTreeSkipMatches(Finder, LengthLimit, CurrentMatch);
            LZP_MATCH_FINDER_ADVANCE(Finder);
            return Offset;
        }
    }

    DistancesEnd = LzpMatchFinderTreeGetMatches(Finder,
                                                LengthLimit,
                                                CurrentMatch,
                                                Distances + Offset,
                                                MaxLength);

    Offset = DistancesEnd - Distances;
    LZP_MATCH_FINDER_ADVANCE(Finder);
    return Offset;
}

VOID
LzpMatchFinderBinTree3Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    )

/*++

Routine Description:

    This routine skips a portion of the input when using the 3 byte binary tree.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Size - Supplies the number of bytes to skip.

Return Value:

    None.

--*/

{

    PCUCHAR Current;
    ULONG CurrentMatch;
    PLZ_REFERENCE Hash;
    ULONG HashValue;
    ULONG HashValue2;
    ULONG LengthLimit;

    Hash = Finder->Hash;
    do {

        //
        // Skip the header.
        //

        LengthLimit = Finder->LengthLimit;
        if (LengthLimit < 3) {
            LZP_MATCH_FINDER_ADVANCE(Finder);
            Size -= 1;
            continue;
        }

        Current = Finder->Buffer;
        LZP_HASH3(Finder, Current, HashValue2, HashValue);
        Hash = Finder->Hash;
        CurrentMatch = Hash[LZMA_HASH3_OFFSET + HashValue];
        Hash[HashValue2] = Finder->Position;
        Hash[LZMA_HASH3_OFFSET + HashValue] = Finder->Position;
        LzpMatchFinderTreeSkipMatches(Finder, LengthLimit, CurrentMatch);
        LZP_MATCH_FINDER_ADVANCE(Finder);
        Size -= 1;

    } while (Size != 0);

    return;
}

ULONG
LzpMatchFinderBinTree4GetMatches (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    )

/*++

Routine Description:

    This routine finds matches using the 4 byte binary tree.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Distances - Supplies a pointer where the distances will be returned on
        success.

Return Value:

    Returns the number of matches times two.

--*/

{

    PCUCHAR Current;
    LZ_REFERENCE CurrentMatch;
    ULONG Distance2;
    ULONG Distance3;
    PULONG DistancesEnd;
    PLZ_REFERENCE Hash;
    ULONG HashValue;
    ULONG HashValue2;
    ULONG HashValue3;
    ULONG LengthLimit;
    PCUCHAR Limit;
    ULONG MaxLength;
    ULONG Offset;
    ULONG Position;
    PCUCHAR Search;

    LengthLimit = Finder->LengthLimit;
    if (LengthLimit < 4) {
        LZP_MATCH_FINDER_ADVANCE(Finder);
        return 0;
    }

    Current = Finder->Buffer;
    LZP_HASH4(Finder, Current, HashValue2, HashValue3, HashValue);
    Hash = Finder->Hash;
    Position = Finder->Position;
    Distance2 = Position - Hash[HashValue2];
    Distance3 = Position - Hash[LZMA_HASH3_OFFSET + HashValue3];
    CurrentMatch = Hash[LZMA_HASH4_OFFSET + HashValue];
    Hash[HashValue2] = Position;
    Hash[LZMA_HASH3_OFFSET + HashValue3] = Position;
    Hash[LZMA_HASH4_OFFSET + HashValue] = Position;
    MaxLength = 0;
    Offset = 0;
    if ((Distance2 < Finder->CyclicBufferSize) &&
        (*(Current - Distance2) == *Current)) {

        MaxLength = 2;
        Distances[0] = 2;
        Distances[1] = Distance2 - 1;
        Offset = 2;
    }

    if ((Distance2 != Distance3) &&
        (Distance3 < Finder->CyclicBufferSize) &&
        (*(Current - Distance3) == *Current)) {

        MaxLength = 3;
        Distances[Offset + 1] = Distance3 - 1;
        Offset += 2;
        Distance2 = Distance3;
    }

    if (Offset != 0) {

        //
        // See how long of a match it is.
        //

        Search = Current + MaxLength;
        Limit = Current + LengthLimit;
        while ((Search != Limit) && (*(Search - Distance2) == *Search)) {
            Search += 1;
        }

        MaxLength = Search - Current;
        Distances[Offset - 2] = MaxLength;
        if (MaxLength == LengthLimit) {
            LzpMatchFinderTreeSkipMatches(Finder, LengthLimit, CurrentMatch);
            LZP_MATCH_FINDER_ADVANCE(Finder);
            return Offset;
        }
    }

    if (MaxLength < 3) {
        MaxLength = 3;
    }

    DistancesEnd = LzpMatchFinderTreeGetMatches(Finder,
                                                LengthLimit,
                                                CurrentMatch,
                                                Distances + Offset,
                                                MaxLength);

    Offset = DistancesEnd - Distances;
    LZP_MATCH_FINDER_ADVANCE(Finder);
    return Offset;
}

VOID
LzpMatchFinderBinTree4Skip (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    )

/*++

Routine Description:

    This routine skips a portion of the input when using the 4 byte binary tree.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Size - Supplies the number of bytes to skip.

Return Value:

    None.

--*/

{

    PCUCHAR Current;
    ULONG CurrentMatch;
    PLZ_REFERENCE Hash;
    ULONG HashValue;
    ULONG HashValue2;
    ULONG HashValue3;
    ULONG LengthLimit;

    Hash = Finder->Hash;
    do {

        //
        // Skip the header.
        //

        LengthLimit = Finder->LengthLimit;
        if (LengthLimit < 4) {
            LZP_MATCH_FINDER_ADVANCE(Finder);
            Size -= 1;
            continue;
        }

        Current = Finder->Buffer;
        LZP_HASH4(Finder, Current, HashValue2, HashValue3, HashValue);
        Hash = Finder->Hash;
        CurrentMatch = Hash[LZMA_HASH4_OFFSET + HashValue];
        Hash[HashValue2] = Finder->Position;
        Hash[LZMA_HASH3_OFFSET + HashValue3] = Finder->Position;
        Hash[LZMA_HASH4_OFFSET + HashValue] = Finder->Position;
        LzpMatchFinderTreeSkipMatches(Finder, LengthLimit, CurrentMatch);
        LZP_MATCH_FINDER_ADVANCE(Finder);
        Size -= 1;

    } while (Size != 0);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
LzpMatchFinderSetDefaults (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine sets the default settings for a newly initialized match finder.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    Finder->CutValue = 32;
    Finder->BinTreeMode = TRUE;
    Finder->HashByteCount = 4;
    Finder->BigHash = FALSE;
    return;
}

VOID
LzpMatchFinderInitializeContext (
    PLZ_MATCH_FINDER Finder,
    BOOL ReadData
    )

/*++

Routine Description:

    This routine initializes a match finder context.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    ReadData - Supplies a boolean indicating whether or not to read the data.

Return Value:

    None.

--*/

{

    PULONG Hash;
    ULONG HashSizeSum;
    ULONG Index;

    HashSizeSum = Finder->HashSizeSum;
    Hash = Finder->Hash;
    for (Index = 0; Index < HashSizeSum; Index += 1) {
        Hash[Index] = LZMA_EMPTY_HASH;
    }

    Finder->CyclicBufferPosition = 0;
    Finder->Buffer = Finder->BufferBase;
    Finder->Position = Finder->CyclicBufferSize;
    Finder->StreamPosition = Finder->CyclicBufferSize;
    Finder->Result = LzSuccess;
    Finder->StreamEndWasReached = FALSE;
    if (ReadData != FALSE) {
        LzpMatchFinderReadBlock(Finder);
    }

    LzpMatchFinderSetLimits(Finder);
    return;
}

VOID
LzpMatchFinderReadBlock (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine fills up the match finder buffer by reading data from the
    input.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    ULONG CurrentSize;
    PUCHAR Destination;
    INTN Size;

    if ((Finder->StreamEndWasReached != FALSE) ||
        (Finder->Result != LzSuccess)) {

        return;
    }

    //
    // If the data is all in memory then just scoot the buffer along.
    //

    if (Finder->DirectInput != FALSE) {
        CurrentSize = LZMA_MAX_VALUE_FOR_NORMALIZE -
                      (Finder->StreamPosition - Finder->Position);

        if (CurrentSize > Finder->DirectInputRemaining) {
            CurrentSize = Finder->DirectInputRemaining;
        }

        Finder->DirectInputRemaining -= CurrentSize;
        Finder->StreamPosition += CurrentSize;
        if (Finder->DirectInputRemaining == 0) {
            Finder->StreamEndWasReached = TRUE;
        }

        return;
    }

    //
    // Loop reading to try and fill up the buffer.
    //

    while (TRUE) {
        Destination = Finder->Buffer +
                      (Finder->StreamPosition - Finder->Position);

        Size = Finder->BufferBase + Finder->BlockSize - Destination;
        if (Size == 0) {
            break;
        }

        //
        // Call the read function if there is one.
        //

        if (Finder->System->Read != NULL) {
            Size = Finder->System->Read(Finder->System, Destination, Size);
            if (Size < 0) {
                Finder->Result = LzErrorRead;
                return;
            }

        //
        // Otherwise, copy from the input buffer.
        //

        } else {
            if (Size > Finder->System->InputSize) {
                Size = Finder->System->InputSize;
                if (Size == 0) {
                    break;
                }
            }

            memcpy(Destination, Finder->System->Input, Size);
            Finder->System->Input += Size;
            Finder->System->InputSize -= Size;
        }

        if (Size == 0) {
            Finder->StreamEndWasReached = TRUE;
            break;
        }

        Finder->System->UncompressedSize += Size;
        Finder->System->UncompressedCrc32 =
                             LzpComputeCrc32(Finder->System->UncompressedCrc32,
                                             Destination,
                                             Size);

        Finder->StreamPosition += Size;
        if (Finder->StreamPosition - Finder->Position > Finder->KeepSizeAfter) {
            break;
        }
    }

    return;
}

PULONG
LzpMatchFinderHashGetMatches (
    PLZ_MATCH_FINDER Finder,
    ULONG LengthLimit,
    ULONG CurrentMatch,
    PULONG Distances,
    ULONG MaxLength
    )

/*++

Routine Description:

    This routine finds additional matches using previous results of the hash
    table.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    LengthLimit - Supplies the match length limit to abide by.

    CurrentMatch - Supplies the current match.

    Distances - Supplies a pointer where the distances will be returned on
        success.

    MaxLength - Supplies the maximum match length found so far.

Return Value:

    Returns a pointer into the distances table just beyond the last filled out
    length/distance pair.

--*/

{

    PCUCHAR Current;
    ULONG CutValue;
    ULONG CyclicBufferPosition;
    ULONG Delta;
    ULONG Index;
    ULONG Length;
    PCUCHAR Search;
    PLZ_REFERENCE Son;

    Current = Finder->Buffer;
    CutValue = Finder->CutValue;
    CyclicBufferPosition = Finder->CyclicBufferPosition;
    Son = Finder->Son;
    Son[CyclicBufferPosition] = CurrentMatch;
    while (TRUE) {
        Delta = Finder->Position - CurrentMatch;
        if ((CutValue == 0) || (Delta >= Finder->CyclicBufferSize)) {
            break;
        }

        CutValue -= 1;
        Search = Current - Delta;
        if (Delta > CyclicBufferPosition) {
            Index = CyclicBufferPosition + Finder->CyclicBufferSize - Delta;

        } else {
            Index = CyclicBufferPosition - Delta;
        }

        CurrentMatch = Son[Index];
        if ((Search[MaxLength] == Current[MaxLength]) &&
            (*Search == *Current)) {

            Length = 1;
            while ((Length != LengthLimit) &&
                   (Search[Length] == Current[Length])) {

                Length += 1;
            }

            if (Length > MaxLength) {
                MaxLength = Length;
                Distances[0] = Length;
                Distances[1] = Delta - 1;
                Distances += 2;
                if (Length == LengthLimit) {
                    return Distances;
                }
            }
        }
    }

    return Distances;
}

PULONG
LzpMatchFinderTreeGetMatches (
    PLZ_MATCH_FINDER Finder,
    ULONG LengthLimit,
    ULONG CurrentMatch,
    PULONG Distances,
    ULONG MaxLength
    )

/*++

Routine Description:

    This routine finds additional matches using previous results of the binary
    tree.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    LengthLimit - Supplies the match length limit to abide by.

    CurrentMatch - Supplies the current match.

    Distances - Supplies a pointer where the distances will be returned on
        success.

    MaxLength - Supplies the maximum match length found so far.

Return Value:

    Returns a pointer into the distances table just beyond the last filled out
    length/distance pair.

--*/

{

    PCUCHAR Current;
    ULONG CutValue;
    ULONG CyclicBufferPosition;
    ULONG Delta;
    ULONG Index;
    ULONG Length;
    ULONG Length0;
    ULONG Length1;
    PLZ_REFERENCE Pair;
    PCUCHAR Search;
    PLZ_REFERENCE Son;
    PLZ_REFERENCE Son0;
    PLZ_REFERENCE Son1;

    Current = Finder->Buffer;
    CutValue = Finder->CutValue;
    CyclicBufferPosition = Finder->CyclicBufferPosition;
    Length0 = 0;
    Length1 = 0;
    Son = Finder->Son;
    Son0 = Son + (CyclicBufferPosition << 1) + 1;
    Son1 = Son + (CyclicBufferPosition << 1);
    while (TRUE) {
        Delta = Finder->Position - CurrentMatch;
        if ((CutValue == 0) || (Delta >= Finder->CyclicBufferSize)) {
            *Son0 = LZMA_EMPTY_HASH;
            *Son1 = LZMA_EMPTY_HASH;
            break;
        }

        CutValue -= 1;
        if (Delta > CyclicBufferPosition) {
            Index = CyclicBufferPosition + Finder->CyclicBufferSize - Delta;

        } else {
            Index = CyclicBufferPosition - Delta;
        }

        Pair = Son + (Index << 1);
        Search = Current - Delta;
        Length = Length0;
        if (Length1 < Length) {
            Length = Length1;
        }

        //
        // Follow a match looking for a new winner.
        //

        if (Search[Length] == Current[Length]) {
            Length += 1;
            while ((Length != LengthLimit) &&
                   (Search[Length] == Current[Length])) {

                Length += 1;
            }

            if (Length > MaxLength) {
                MaxLength = Length;
                Distances[0] = Length;
                Distances[1] = Delta - 1;
                Distances += 2;
                if (Length == LengthLimit) {
                    *Son1 = Pair[0];
                    *Son0 = Pair[1];
                    break;
                }
            }
        }

        if (Search[Length] < Current[Length]) {
            *Son1 = CurrentMatch;
            Son1 = Pair + 1;
            CurrentMatch = *Son1;
            Length1 = Length;

        } else {
            *Son0 = CurrentMatch;
            Son0 = Pair;
            CurrentMatch = *Son0;
            Length0 = Length;
        }
    }

    return Distances;
}

VOID
LzpMatchFinderTreeSkipMatches (
    PLZ_MATCH_FINDER Finder,
    ULONG LengthLimit,
    ULONG CurrentMatch
    )

/*++

Routine Description:

    This routine skips matches when using the binary tree match finder.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    LengthLimit - Supplies the match length limit to abide by.

    CurrentMatch - Supplies the current match.

Return Value:

    None.

--*/

{

    PCUCHAR Current;
    ULONG CutValue;
    ULONG CyclicBufferPosition;
    ULONG Delta;
    ULONG Index;
    ULONG Length;
    ULONG Length0;
    ULONG Length1;
    PLZ_REFERENCE Pair;
    PCUCHAR Search;
    PLZ_REFERENCE Son;
    PLZ_REFERENCE Son0;
    PLZ_REFERENCE Son1;

    Current = Finder->Buffer;
    CutValue = Finder->CutValue;
    CyclicBufferPosition = Finder->CyclicBufferPosition;
    Length0 = 0;
    Length1 = 0;
    Son = Finder->Son;
    Son0 = Son + (CyclicBufferPosition << 1) + 1;
    Son1 = Son + (CyclicBufferPosition << 1);
    while (TRUE) {
        Delta = Finder->Position - CurrentMatch;
        if ((CutValue == 0) || (Delta >= Finder->CyclicBufferSize)) {
            *Son0 = LZMA_EMPTY_HASH;
            *Son1 = LZMA_EMPTY_HASH;
            break;
        }

        CutValue -= 1;
        if (Delta > CyclicBufferPosition) {
            Index = CyclicBufferPosition + Finder->CyclicBufferSize - Delta;

        } else {
            Index = CyclicBufferPosition - Delta;
        }

        Pair = Son + (Index << 1);
        Search = Current - Delta;
        Length = Length0;
        if (Length1 < Length) {
            Length = Length1;
        }

        //
        // Follow a match.
        //

        if (Search[Length] == Current[Length]) {
            Length += 1;
            while ((Length != LengthLimit) &&
                   (Search[Length] == Current[Length])) {

                Length += 1;
            }

            if (Length == LengthLimit) {
                *Son1 = Pair[0];
                *Son0 = Pair[1];
                break;
            }
        }

        if (Search[Length] < Current[Length]) {
            *Son1 = CurrentMatch;
            Son1 = Pair + 1;
            CurrentMatch = *Son1;
            Length1 = Length;

        } else {
            *Son0 = CurrentMatch;
            Son0 = Pair;
            CurrentMatch = *Son0;
            Length0 = Length;
        }
    }

    return;
}

VOID
LzpMatchFinderCheckLimits (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine checks the limits of the match finder.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    if (Finder->Position == LZMA_MAX_VALUE_FOR_NORMALIZE) {
        LzpMatchFinderNormalize(Finder);
    }

    if ((Finder->StreamEndWasReached == FALSE) &&
        (Finder->StreamPosition - Finder->Position <= Finder->KeepSizeAfter)) {

        if (LZP_MATCH_FINDER_NEEDS_MOVE(Finder)) {
            LzpMatchFinderMoveBlock(Finder);
        }

        LzpMatchFinderReadBlock(Finder);
    }

    if (Finder->CyclicBufferPosition == Finder->CyclicBufferSize) {
        Finder->CyclicBufferPosition = 0;
    }

    LzpMatchFinderSetLimits(Finder);
    return;
}

VOID
LzpMatchFinderSetLimits (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine sets up the match finder search limit.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    ULONG LengthLimit;
    ULONG Limit;
    ULONG Minimum;

    //
    // Start by going all the way to the max 32 bit position. Back off to the
    // remaining cyclic buffer.
    //

    Minimum = LZMA_MAX_VALUE_FOR_NORMALIZE - Finder->Position;
    Limit = Finder->CyclicBufferSize - Finder->CyclicBufferPosition;
    if (Limit < Minimum) {
        Minimum = Limit;
    }

    Limit = Finder->StreamPosition - Finder->Position;
    if (Limit <= Finder->KeepSizeAfter) {
        if (Limit > 0) {
            Limit = 1;
        }

    } else {
        Limit -= Finder->KeepSizeAfter;
    }

    if (Limit < Minimum) {
        Minimum = Limit;
    }

    //
    // Set up the length limit.
    //

    LengthLimit = Finder->StreamPosition - Finder->Position;
    if (LengthLimit > Finder->MatchMaxLength) {
        LengthLimit = Finder->MatchMaxLength;
    }

    Finder->LengthLimit = LengthLimit;
    Finder->PositionLimit = Finder->Position + Minimum;
    return;
}

VOID
LzpMatchFinderNormalize (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine normalizes the match finder hash table.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    ULONG Value;

    Value = (Finder->Position - Finder->HistorySize - 1) & LZMA_NORMALIZE_MASK;
    LzpMatchFinderNormalizeTable(Value, Finder->Hash, Finder->ReferenceCount);
    LZP_MATCH_FINDER_REDUCE_OFFSETS(Finder, Value);
    return;
}

VOID
LzpMatchFinderNormalizeTable (
    ULONG Value,
    PLZ_REFERENCE Table,
    UINTN Count
    )

/*++

Routine Description:

    This routine reduces every element in the given table by the requested
    amount.

Arguments:

    Value - Supplies the amount to reduce each element by.

    Table - Supplies the table of elements to reduce.

    Count - Supplies the number of elements in the table.

Return Value:

    None.

--*/

{

    UINTN Index;

    for (Index = 0; Index < Count; Index += 1) {
        if (Table[Index] <= Value) {
            Table[Index] = LZMA_EMPTY_HASH;

        } else {
            Table[Index] -= Value;
        }
    }

    return;
}

VOID
LzpMatchFinderMoveBlock (
    PLZ_MATCH_FINDER Finder
    )

/*++

Routine Description:

    This routine moves the remaining input to the beginning of the buffer.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

{

    UINTN Size;

    Size = (UINTN)(Finder->StreamPosition - Finder->Position) +
           Finder->KeepSizeBefore;

    memmove(Finder->BufferBase,
            Finder->Buffer - Finder->KeepSizeBefore,
            Size);

    Finder->Buffer = Finder->BufferBase + Finder->KeepSizeBefore;
    return;
}


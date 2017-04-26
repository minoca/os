/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzfind.h

Abstract:

    This header contains definitions for the match finder in the LZMA encoder.

Author:

    Evan Green 27-Oct-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_MAX_HISTORY_SIZE (3 << 29)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONG LZ_REFERENCE, *PLZ_REFERENCE;

/*++

Structure Description:

    This structure stores the state for the match finder in the LZMA encoder.

Members:

    Buffer - Stores the current input buffer pointer.

    Position - Stores the input position where the current buffer is pointing.

    PositionLimit - Stores the position limit.

    StreamPosition - Stores the position within the stream of the end of the
        current buffer.

    LengthLimit - Stores the match length limit.

    CyclicBufferPosition - Stores the current position within the cyclic buffer.

    CyclicBufferSize - Stores the size of the cyclic buffer.

    StreamEndWasReached - Stores a boolean indicating if the end of the stream
        has been read or not.

    BinTreeMode - Stores a boolean indicating whether to use binary tree mode
        (TRUE) or hash table mode (FALSE).

    BigHash - Stores a boolean indicating whether or not to use the big hash
        table.

    DirectInput - Stores a boolean indicating whether the input came directly
        from the caller and should not be freed.

    MatchLengthMax - Stores the maximum length of a match.

    Hash - Store the hash table of matches.

    Son - Stores the son table.

    HashMask - Stores the bitmask to use based on the hash table size.

    CutValue - Stores the cut value, also known as the match count. It is the
        number of match finder cycles to perform on each iteration.

    BufferBase - Stores the original buffer address.

    System - Stores a pointer to the external context.

    BlockSize - Stores the size of a block.

    KeepSizeBefore - Stores the number of bytes before the current buffer
        position to hold on to.

    KeepSizeAfter - Stores the number of bytes after the current buffer
        position to hold on to.

    HashByteCount - Stores the number of bytes in a hash key.

    DirectInputRemaining - Stores the number of bytes remaining in the user
        supplied buffer.

    HistorySize - Stores the size of history to keep.

    FixedHashSize - Stores the fixed hash table size.

    HashSizeSum - Stores the sum of the hash table sizes.

    Result - Stores the resulting status code.

    ReferenceCount - Stores the total number of elements in the giant flat
        array of hash tables and the son array.

--*/

typedef struct _LZ_MATCH_FINDER {
    PUCHAR Buffer;
    ULONG Position;
    ULONG PositionLimit;
    ULONG StreamPosition;
    ULONG LengthLimit;
    ULONG CyclicBufferPosition;
    ULONG CyclicBufferSize;
    BOOL StreamEndWasReached;
    BOOL BinTreeMode;
    BOOL BigHash;
    BOOL DirectInput;
    ULONG MatchMaxLength;
    PLZ_REFERENCE Hash;
    PLZ_REFERENCE Son;
    ULONG HashMask;
    ULONG CutValue;
    PUCHAR BufferBase;
    PLZ_CONTEXT System;
    ULONG BlockSize;
    ULONG KeepSizeBefore;
    ULONG KeepSizeAfter;
    ULONG HashByteCount;
    UINTN DirectInputRemaining;
    ULONG HistorySize;
    ULONG FixedHashSize;
    ULONG HashSizeSum;
    LZ_STATUS Result;
    UINTN ReferenceCount;
} LZ_MATCH_FINDER, *PLZ_MATCH_FINDER;

typedef
VOID
(*PLZ_MATCH_FINDER_INITIALIZE) (
    PLZ_MATCH_FINDER Finder
    );

/*++

Routine Description:

    This routine initializes a match finder context.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

typedef
LZ_STATUS
(*PLZ_MATCH_FINDER_LOAD) (
    PLZ_MATCH_FINDER Finder,
    LZ_FLUSH_OPTION Flush
    );

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

typedef
ULONG
(*PLZ_MATCH_FINDER_GET_COUNT) (
    PLZ_MATCH_FINDER Finder
    );

/*++

Routine Description:

    This routine returns the number of available bytes in the finder context.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    Returns the number of available bytes.

--*/

typedef
PCUCHAR
(*PLZ_MATCH_FINDER_GET_POSITION) (
    PLZ_MATCH_FINDER Finder
    );

/*++

Routine Description:

    This routine returns a pointer to the current position in the match finder
    context.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    Returns a pointer to the current position within the source.

--*/

typedef
ULONG
(*PLZ_MATCH_FINDER_GET_MATCHES) (
    PLZ_MATCH_FINDER Finder,
    PULONG Distances
    );

/*++

Routine Description:

    This routine finds matches.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Distances - Supplies a pointer where the distances will be returned on
        success.

Return Value:

    Returns the number of matches times two (since they come in pairs).

--*/

typedef
VOID
(*PLZ_MATCH_FINDER_SKIP) (
    PLZ_MATCH_FINDER Finder,
    ULONG Size
    );

/*++

Routine Description:

    This routine skips a portion of the input.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Size - Supplies the number of bytes to skip.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores the function table for the match finder.

Members:

    Initialize - Stores a pointer to a function used to initialize a match
        finder interface.

    Load - Stores a pointer to a function used to load input data into the
        match finder.

    GetCount - Stores a pointer to a function used to to get the number of
        available bytes in the match finder.

    GetPosition - Stores a pointer to a function used to return a pointer to
        the current position in the source.

    GetMatches - Stores a pointer to a function used to get matches from the
        match finder.

    Skip - Stores a pointer to a function used to skip source data.

--*/

typedef struct _LZ_MATCH_FINDER_INTERFACE {
    PLZ_MATCH_FINDER_INITIALIZE Initialize;
    PLZ_MATCH_FINDER_LOAD Load;
    PLZ_MATCH_FINDER_GET_COUNT GetCount;
    PLZ_MATCH_FINDER_GET_POSITION GetPosition;
    PLZ_MATCH_FINDER_GET_MATCHES GetMatches;
    PLZ_MATCH_FINDER_SKIP Skip;
} LZ_MATCH_FINDER_INTERFACE, *PLZ_MATCH_FINDER_INTERFACE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
LzpInitializeMatchFinder (
    PLZ_MATCH_FINDER Finder
    );

/*++

Routine Description:

    This routine initializes an LZMA match finder.

Arguments:

    Finder - Supplies a pointer to the match finder context.

Return Value:

    None.

--*/

VOID
LzpDestroyMatchFinder (
    PLZ_MATCH_FINDER Finder,
    PLZ_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys an LZMA match finder.

Arguments:

    Finder - Supplies a pointer to the match finder context.

    Context - Supplies a pointer to the system context.

Return Value:

    None.

--*/

LZ_STATUS
LzpMatchFinderAllocateBuffers (
    PLZ_MATCH_FINDER Finder,
    ULONG HistorySize,
    ULONG KeepAddBufferBefore,
    ULONG MatchMaxLength,
    ULONG KeepAddBufferAfter,
    PLZ_CONTEXT Context
    );

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

VOID
LzpMatchFinderInitializeInterface (
    PLZ_MATCH_FINDER Finder,
    PLZ_MATCH_FINDER_INTERFACE Interface
    );

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


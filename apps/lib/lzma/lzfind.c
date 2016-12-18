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
#include "lzfind.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_CRC_POLYNOMIAL 0xEDB88320

#define LZMA_HASH2_SIZE (1 << 10)
#define LZMA_HASH3_SIZE (1 << 16)
#define LZMA_HASH4_SIZE (1 << 20)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
LzpMatchFinderSetDefaults (
    PLZ_MATCH_FINDER Finder
    );

//
// -------------------------------------------------------------------- Globals
//

//
// TODO: Write these functions.
//

const LZ_MATCH_FINDER_INTERFACE LzDefaultMatchFinderInterface = {
    NULL, // LzpMatchFinderInitialize,
    NULL, // LzpMatchFinderGetAvailableBytes,
    NULL, // LzpMatchFinderGetPosition,
    NULL, // LzpMatchFinderBinTree4GetMatches,
    NULL, // LzpMatchFinderBinTree4Skip
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

    ULONG Bit;
    ULONG Index;
    ULONG Rotate;

    Finder->BufferBase = NULL;
    Finder->DirectInput = FALSE;
    Finder->Hash = NULL;
    LzpMatchFinderSetDefaults(Finder);
    for (Index = 0; Index < 0x100; Index += 1) {
        Rotate = Index;
        for (Bit = 0; Bit < 8; Bit += 1) {
            Rotate = (Rotate >> 1) ^
                     (LZMA_CRC_POLYNOMIAL & ~((Rotate & 0x1) - 1));
        }

        Finder->Crc[Index] = Rotate;
    }

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
        if (HashMask > (1 >> 24)) {
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
        }

        AllocationSize = NewSize * sizeof(LZ_REFERENCE);
        if (AllocationSize / sizeof(LZ_REFERENCE) != NewSize) {
            return LzErrorMemory;
        }

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

    //
    // TODO: Enable this when these functions are implemented.
    //

#if 0

    if (Finder->BinTreeMode == FALSE) {
        Interface->GetCount = LzpMatchFinderHash4GetAvailableBytes;
        Interface->Skip = LzpMatchFinderHash4Skip;

    } else {
        if (Finder->HashByteCount == 2) {
            Interface->GetCount = LzpMatchFinderBinTree2GetAvailableBytes;
            Interface->Skip = LzpMatchFinderBinTree2Skip;

        } else if (Finder->HashByteCount == 3) {
            Interface->GetCount = LzpMatchFinderBinTree3GetAvailableBytes;
            Interface->Skip = LzpMatchFinderBinTree3Skip;
        }
    }

#endif

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


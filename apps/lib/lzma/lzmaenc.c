/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzmaenc.c

Abstract:

    This module implements the LZMA encoder. This is a port of Igor Pavlov's
    7z encoder.

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
#include "lzmaenc.h"

//
// --------------------------------------------------------------------- Macros
//

//
// The macro returns the number of bytes the range encoder has processed.
//

#define LzpRangeEncoderGetProcessed(_RangeEncoder) \
    ((_RangeEncoder)->System->CompressedSize + \
     ((_RangeEncoder)->Buffer - (_RangeEncoder)->BufferRead) + \
     (_RangeEncoder)->CacheSize)

//
// Continue to forge ahead on account of the input if either:
// 1) The input is finished (indicated by flush or found naturally).
// 2) There is a read function.
// 3) The number of bytes in the match finder buffer is greater than the
//    "keep size after" value (so that a maximum length can be matched against).
//

#define LZMA_HAS_INPUT_SPACE(_Encoder, _Flush) \
    (((_Flush) != LzNoFlush) || \
     ((_Encoder)->MatchFinderData.System->Read != NULL) || \
     ((_Encoder)->MatchFinderData.StreamEndWasReached != FALSE) || \
     (((_Encoder)->MatchFinder.GetCount((_Encoder)->MatchFinderContext) + \
       (_Encoder)->RangeEncoder.System->InputSize) >= \
      (_Encoder)->MatchFinderData.KeepSizeAfter))

//
// Continue to forge ahead on account of the output if either:
// 1) Completely flushing now.
// 2) There is a write function.
// 3) There is enough buffer space in the range encoder to encode the longest
//    possible symbol.
//

#define LZMA_HAS_OUTPUT_SPACE(_Encoder, _Flush) \
    (((_Flush) == LzFlushNow) || \
     ((_Encoder)->RangeEncoder.System->Write != NULL) || \
     (((_Encoder)->RangeEncoder.BufferLimit - \
       (_Encoder)->RangeEncoder.Buffer) >= LZMA_MAX_INPUT))

#define LZMA_HAS_BUFFER_SPACE(_Encoder, _Flush) \
    ((LZMA_HAS_INPUT_SPACE(_Encoder, _Flush)) && \
     (LZMA_HAS_OUTPUT_SPACE(_Encoder, _Flush)))

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_BIG_HASH_DICT_LIMIT (1 << 24)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PLZMA_ENCODER
LzpLzmaCreateEncoder (
    PLZ_CONTEXT Context
    );

VOID
LzpLzmaDestroyEncoder (
    PLZMA_ENCODER Encoder,
    PLZ_CONTEXT Context
    );

VOID
LzpLzmaDestroyLiterals (
    PLZMA_ENCODER Encoder,
    PLZ_CONTEXT Context
    );

LZ_STATUS
LzpLzmaEncoderSetProperties (
    PLZMA_ENCODER Encoder,
    PLZMA_ENCODER_PROPERTIES Properties
    );

LZ_STATUS
LzpLzmaWriteProperties (
    PLZMA_ENCODER Encoder,
    PUCHAR Properties,
    PUINTN PropertiesSize
    );

VOID
LzpLzmaNormalizeProperties (
    PLZMA_ENCODER_PROPERTIES Properties
    );

VOID
LzpLzmaInitializeFastPosition (
    PUCHAR FastPosition
    );

VOID
LzpLzmaInitializePriceTables (
    PULONG Prices
    );

LZ_STATUS
LzpLzmaAllocateBuffers (
    PLZMA_ENCODER Encoder,
    ULONG KeepWindowSize,
    PLZ_CONTEXT Context
    );

VOID
LzpLzmaResetEncoder (
    PLZMA_ENCODER Encoder
    );

VOID
LzpLzmaResetPrices (
    PLZMA_ENCODER Encoder
    );

VOID
LzpLzmaFillDistancesPrices (
    PLZMA_ENCODER Encoder
    );

VOID
LzpLzmaFillAlignPrices (
    PLZMA_ENCODER Encoder
    );

LZ_STATUS
LzpLzmaEncode (
    PLZMA_ENCODER Encoder,
    BOOL UseLimits,
    ULONG MaxPackSize,
    ULONG MaxUnpackSize,
    LZ_FLUSH_OPTION Flush
    );

LZ_STATUS
LzpLzmaEncoderFlush (
    PLZMA_ENCODER Encoder,
    ULONG Position
    );

VOID
LzpLzmaEncoderWriteEndMark (
    PLZMA_ENCODER Encoder,
    ULONG PositionState
    );

LZ_STATUS
LzpLzmaEncoderGetError (
    PLZMA_ENCODER Encoder
    );

VOID
LzpLengthEncoderEncodeAndUpdate (
    PLZMA_LENGTH_PRICE_ENCODER LengthPrice,
    PLZMA_RANGE_ENCODER Range,
    ULONG Symbol,
    ULONG PositionState,
    BOOL UpdatePrice,
    PULONG ProbabilityPrices
    );

VOID
LzpLengthPriceEncoderUpdateTables (
    PLZMA_LENGTH_PRICE_ENCODER LengthPrice,
    ULONG PositionStateCount,
    PULONG ProbabilityPrices
    );

VOID
LzpLengthPriceEncoderUpdateTable (
    PLZMA_LENGTH_PRICE_ENCODER LengthPrice,
    ULONG PositionState,
    PULONG ProbabilityPrices
    );

VOID
LzpLengthEncoderInitialize (
    PLZMA_LENGTH_ENCODER LengthEncoder
    );

VOID
LzpLengthEncoderEncode (
    PLZMA_LENGTH_ENCODER LengthEncoder,
    PLZMA_RANGE_ENCODER RangeEncoder,
    ULONG Symbol,
    ULONG PositionState
    );

VOID
LzpLengthEncoderSetPrices (
    PLZMA_LENGTH_ENCODER Length,
    ULONG PositionState,
    ULONG SymbolCount,
    PULONG Prices,
    PULONG ProbabilityPrices
    );

VOID
LzpRcTreeEncode (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol
    );

VOID
LzpRcTreeReverseEncode (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol
    );

ULONG
LzpRcTreeGetPrice (
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol,
    PULONG Prices
    );

ULONG
LzpRcTreeReverseGetPrice (
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol,
    PULONG Prices
    );

VOID
LzpLiteralEncoderEncode (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    ULONG Symbol
    );

VOID
LzpLiteralEncoderEncodeMatched (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    ULONG Symbol,
    ULONG MatchByte
    );

LZ_STATUS
LzpRangeEncoderInitialize (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_CONTEXT Context
    );

VOID
LzpRangeEncoderDestroy (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_CONTEXT Context
    );

VOID
LzpRangeEncoderReset (
    PLZMA_RANGE_ENCODER RangeEncoder
    );

VOID
LzpRangeEncodeDirectBits (
    PLZMA_RANGE_ENCODER RangeEncoder,
    ULONG Value,
    ULONG BitCount
    );

VOID
LzpRangeEncodeBit (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    ULONG Symbol
    );

VOID
LzpRangeEncoderFlushData (
    PLZMA_RANGE_ENCODER RangeEncoder
    );

VOID
LzpRangeEncoderShiftLow (
    PLZMA_RANGE_ENCODER RangeEncoder
    );

VOID
LzpRangeEncoderFlushStream (
    PLZMA_RANGE_ENCODER RangeEncoder
    );

BOOL
LzpLzmaCopyOutput (
    PLZ_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

const UCHAR LzLzmaLiteralNextStates[LZMA_STATE_COUNT] = {
    0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 4, 5
};

const UCHAR LzLzmaMatchNextStates[LZMA_STATE_COUNT] = {
    7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 10, 10
};

const UCHAR LzLzmaRepNextStates[LZMA_STATE_COUNT] = {
    8, 8, 8, 8, 8, 8, 8, 11, 11, 11, 11, 11
};

const UCHAR LzLzmaShortRepNextStates[LZMA_STATE_COUNT] = {
    9, 9, 9, 9, 9, 9, 9, 11, 11, 11, 11, 11
};

//
// ------------------------------------------------------------------ Functions
//

VOID
LzLzmaInitializeProperties (
    PLZMA_ENCODER_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine initializes LZMA properties to their defaults.

Arguments:

    Properties - Supplies a pointer to the properties to initialize.

Return Value:

    None.

--*/

{

    Properties->Level = 5;
    Properties->DictionarySize = 0;
    Properties->MatchCount = 0;
    Properties->ReduceSize = -1ULL;
    Properties->Lc = -1;
    Properties->Lp = -1;
    Properties->Pb = -1;
    Properties->Algorithm = -1;
    Properties->FastBytes = -1;
    Properties->BinTreeMode = -1;
    Properties->HashByteCount = -1;
    Properties->ThreadCount = -1;
    Properties->EndMark = TRUE;
    return;
}

LZ_STATUS
LzLzmaInitializeEncoder (
    PLZ_CONTEXT Context,
    PLZMA_ENCODER_PROPERTIES Properties,
    BOOL FileWrapper
    )

/*++

Routine Description:

    This routine initializes a given LZ context for encoding. The structure
    should be zeroed and initialized before this function is called. If the
    read/write functions are going to be used, they should already be non-null.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

    Properties - Supplies an optional pointer to the properties to set in the
        encoder. If NULL is supplied, default properties will be set that are
        equivalent to compression level five.

    FileWrapper - Supplies a boolean indicating if the file header and footer
        should be written to the output stream.

Return Value:

    LZ Status code.

--*/

{

    PLZMA_ENCODER Encoder;
    BOOL EncoderCreated;
    LZ_STATUS Status;

    Encoder = NULL;
    EncoderCreated = FALSE;
    if (Context->Reallocate == NULL) {
        return LzErrorInvalidParameter;
    }

    Context->CompressedCrc32 = 0;
    Context->UncompressedCrc32 = 0;
    Context->CompressedSize = 0;
    Context->UncompressedSize = 0;
    if (Context->InternalState == NULL) {
        Context->InternalState = LzpLzmaCreateEncoder(Context);
        if (Context->InternalState == NULL) {
            return LzErrorMemory;
        }

        EncoderCreated = TRUE;
    }

    Encoder = Context->InternalState;
    Encoder->FileWrapper = FileWrapper;
    if (FileWrapper != FALSE) {
        Encoder->Stage = LzmaStageFileHeader;

    } else {
        Encoder->Stage = LzmaStageData;
    }

    if (Properties != NULL) {
        Status = LzpLzmaEncoderSetProperties(Encoder, Properties);
        if (Status != LzSuccess) {
            goto InitializeEncoderEnd;
        }
    }

    Encoder->NeedInitialization = TRUE;
    Status = LzpLzmaAllocateBuffers(Encoder, 0, Context);
    Encoder->MatchFinderData.System = Context;
    Encoder->RangeEncoder.System = Context;

InitializeEncoderEnd:
    if (Status != LzSuccess) {
        if (EncoderCreated != FALSE) {
            LzpLzmaDestroyEncoder(Context->InternalState, Context);
            Context->InternalState = NULL;
        }
    }

    return Status;
}

LZ_STATUS
LzLzmaEncode (
    PLZ_CONTEXT Context,
    LZ_FLUSH_OPTION Flush
    )

/*++

Routine Description:

    This routine encodes the given initialized LZMA stream.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

    Flush - Supplies the flush option, which indicates whether the encoder
        should be flushed and terminated with this call or not.

Return Value:

    LZ Status code.

--*/

{

    PLZMA_ENCODER Encoder;
    ULONG Magic;
    PVOID OldOutBuffer;
    UINTN OutSize;
    PLZMA_RANGE_ENCODER Range;
    LZ_STATUS Status;

    Encoder = Context->InternalState;
    OldOutBuffer = NULL;
    Range = &(Encoder->RangeEncoder);

    //
    // If this is the only time the encode function is being called, then
    // encode directly to memory if no read/write functions are supplied.
    //

    if (Context->UncompressedSize == 0) {
        if ((Flush != LzNoFlush) && (Context->Read == NULL)) {
            Context->Reallocate(Encoder->MatchFinderData.BufferBase, 0);
            Encoder->MatchFinderData.BufferBase = (PUCHAR)(Context->Input);
            Encoder->MatchFinderData.DirectInputRemaining = Context->InputSize;
            Encoder->MatchFinderData.DirectInput = TRUE;
        }
    }

    //
    // If there's no leftover data in the allocated output buffer and the
    // supplied one is large enough, use it directly.
    //

    if ((Context->Write == NULL) && (LzpLzmaCopyOutput(Context) != FALSE) &&
        (Context->OutputSize >= LZMA_MAX_INPUT) &&
        (Range->DirectOutput == FALSE)) {

        OldOutBuffer = Range->Buffer;
        Range->Buffer = Context->Output;
        Range->BufferLimit = Context->Output + Context->OutputSize;
        Range->BufferRead = Context->Output;
        Range->BufferBase = Context->Output;
        Range->DirectOutput = TRUE;
    }

    //
    // If writing the header out, do that now. Either the range encoder buffer
    // was allocated internally, in which case it's definitely big enough, or
    // it's the user's, in which case they only get one shot for the whole
    // thing to be big enough anyway.
    //

    if (Encoder->Stage == LzmaStageFileHeader) {
        if (Range->BufferLimit - Range->Buffer < LZMA_HEADER_SIZE) {
            Status = LzErrorOutputEof;
            goto EncodeEnd;
        }

        Magic = LZMA_HEADER_MAGIC;
        memcpy(Range->Buffer, &Magic, LZMA_HEADER_MAGIC_SIZE);
        Range->Buffer += LZMA_HEADER_MAGIC_SIZE;
        OutSize = LZMA_HEADER_SIZE - LZMA_HEADER_MAGIC_SIZE;
        Status = LzpLzmaWriteProperties(Encoder, Range->Buffer, &OutSize);
        if (Status != LzSuccess) {
            goto EncodeEnd;
        }

        Range->Buffer += OutSize;
        Encoder->Stage = LzmaStageData;
    }

    //
    // Potentially encode some input data.
    //

    if (Encoder->Stage == LzmaStageData) {
        Status = LzpLzmaEncode(Encoder, FALSE, 0, 0, Flush);
        if (Status != LzSuccess) {
            if (Status == LzErrorProgress) {
                Status = LzSuccess;
            }

            goto EncodeEnd;
        }

        if (Encoder->MatchFinderData.StreamEndWasReached != FALSE) {
            Status = LzpLzmaEncoderFlush(Encoder, (ULONG)(Encoder->Processed));
            if (Status != LzSuccess) {
                goto EncodeEnd;
            }

            Encoder->Stage = LzmaStageFlushingOutput;
        }
    }

    //
    // Continue to push the remaining output. Input is finished. With a write
    // function there's actually nothing to do. Potentially move on to the
    // footer.
    //

    if (Encoder->Stage == LzmaStageFlushingOutput) {
        if ((Context->Write != NULL) || (LzpLzmaCopyOutput(Context) != FALSE)) {
            if (Encoder->FileWrapper != FALSE) {
                Encoder->Stage = LzmaStageFileFooter;
                if (Range->BufferLimit - Range->Buffer < LZMA_FOOTER_SIZE) {
                    Status = LzErrorOutputEof;
                    goto EncodeEnd;
                }

                memcpy(&(Range->Buffer[0]),
                       &(Context->UncompressedSize),
                       sizeof(ULONGLONG));

                memcpy(&(Range->Buffer[8]),
                       &(Context->CompressedCrc32),
                       sizeof(ULONG));

                memcpy(&(Range->Buffer[12]),
                       &(Context->UncompressedCrc32),
                       sizeof(ULONG));

                Range->Buffer += LZMA_FOOTER_SIZE;

            } else {
                Encoder->Stage = LzmaStageComplete;
            }
        }
    }

    //
    // Write the check fields out if it's time for the file footer.
    //

    if (Encoder->Stage == LzmaStageFileFooter) {
        if (Context->Write != NULL) {
            if (Context->Write(Context, Range->BufferRead, LZMA_FOOTER_SIZE) !=
                LZMA_FOOTER_SIZE) {

                Status = LzErrorWrite;
                goto EncodeEnd;
            }

            Encoder->Stage = LzmaStageComplete;

        } else {
            if (LzpLzmaCopyOutput(Context) != FALSE) {
                Encoder->Stage = LzmaStageComplete;
            }
        }
    }

    Status = LzSuccess;
    if (Encoder->Stage == LzmaStageComplete) {
        Status = LzStreamComplete;
    }

EncodeEnd:

    //
    // Put the originally allocated output buffer back if it had been hijacked.
    //

    if (OldOutBuffer != NULL) {
        LzpLzmaCopyOutput(Context);
        Range->Buffer = OldOutBuffer;
        Range->BufferLimit = OldOutBuffer + LZMA_RANGE_ENCODER_BUFFER_SIZE;
        Range->BufferBase = OldOutBuffer;
        Range->BufferRead = OldOutBuffer;
        Range->DirectOutput = FALSE;
    }

    return Status;
}

LZ_STATUS
LzLzmaFinishEncode (
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine flushes the LZMA encoder, and potentially writes the ending
    CRC and length fields.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

Return Value:

    LZ Status code.

--*/

{

    PLZMA_ENCODER Encoder;
    LZ_STATUS Status;

    Encoder = Context->InternalState;
    Status = LzLzmaEncode(Context, LzFlushNow);
    LzpLzmaDestroyEncoder(Encoder, Context);
    Context->InternalState = NULL;
    return Status;
}

//
// Functions internal to the encoder that are referenced by other encoder files.
//

ULONG
LzpLzmaReadMatchDistances (
    PLZMA_ENCODER Encoder,
    PULONG DistancePairCount
    )

/*++

Routine Description:

    This routine finds the longest match in the previous input.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    DistancePairCount - Supplies a pointer where the number of match distance
        pairs will be returned.

Return Value:

    Returns the length of the longest match.

--*/

{

    ULONG Available;
    PCUCHAR Current;
    INTN Difference;
    PCUCHAR End;
    ULONG LengthResult;
    ULONG PairCount;
    PCUCHAR Search;

    LengthResult = 0;
    Encoder->AvailableCount =
                    Encoder->MatchFinder.GetCount(Encoder->MatchFinderContext);

    PairCount = Encoder->MatchFinder.GetMatches(Encoder->MatchFinderContext,
                                                Encoder->Matches);

    if (PairCount > 0) {
        LengthResult = Encoder->Matches[PairCount - 2];
        if (LengthResult == Encoder->FastByteCount) {
            Available = Encoder->AvailableCount;
            if (Available > LZMA_MAX_MATCH_LENGTH) {
                Available = LZMA_MAX_MATCH_LENGTH;
            }

            Current =
                Encoder->MatchFinder.GetPosition(Encoder->MatchFinderContext) -
                1;

            Search = Current + LengthResult;
            Difference = -1L - Encoder->Matches[PairCount - 1];
            End = Current + Available;
            while ((Search != End) && (Search[0] == Search[Difference])) {
                Search += 1;
            }

            LengthResult = Search - Current;
        }
    }

    Encoder->AdditionalOffset += 1;
    *DistancePairCount = PairCount;
    return LengthResult;
}

ULONG
LzpLzmaGetPositionSlot (
    PLZMA_ENCODER Encoder,
    ULONG Position
    )

/*++

Routine Description:

    This routine returns the slot associated with the given position.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Position - Supplies the current position.

Return Value:

    Returns the slot associated with the given position.

--*/

{

    ULONG Shift;

    if (Position < LZMA_FULL_DISTANCES) {
        return Encoder->FastPosition[Position];
    }

    if (Position < (1 << (LZMA_DICT_LOG_BITS + 6))) {
        Shift = 6;

    } else {
        Shift = 6 + LZMA_DICT_LOG_BITS - 1;
    }

    return Encoder->FastPosition[Position >> Shift] + (Shift * 2);
}

ULONG
LzpLzmaGetPositionSlot2 (
    PLZMA_ENCODER Encoder,
    ULONG Position
    )

/*++

Routine Description:

    This routine returns the slot associated with the given position.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Position - Supplies the current position.

Return Value:

    Returns the slot associated with the given position.

--*/

{

    ULONG Shift;

    if (Position < (1 << (LZMA_DICT_LOG_BITS + 6))) {
        Shift = 6;

    } else {
        Shift = 6 + LZMA_DICT_LOG_BITS - 1;
    }

    return Encoder->FastPosition[Position >> Shift] + (Shift * 2);
}

//
// --------------------------------------------------------- Internal Functions
//

PLZMA_ENCODER
LzpLzmaCreateEncoder (
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine allocates an LZMA encoder context.

Arguments:

    Context - Supplies a pointer to the general LZ context.

Return Value:

    Returns a pointer to the LZMA encoder on success.

    NULL on allocation failure.

--*/

{

    PLZMA_ENCODER Encoder;
    LZMA_ENCODER_PROPERTIES Properties;

    LzpCrcInitialize();
    Encoder = Context->Reallocate(NULL, sizeof(LZMA_ENCODER));
    if (Encoder == NULL) {
        return NULL;
    }

    memset(Encoder, 0, sizeof(LZMA_ENCODER));
    LzpInitializeMatchFinder(&(Encoder->MatchFinderData));
    LzLzmaInitializeProperties(&Properties);
    LzpLzmaEncoderSetProperties(Encoder, &Properties);
    LzpLzmaInitializeFastPosition(Encoder->FastPosition);
    LzpLzmaInitializePriceTables(Encoder->ProbabilityPrices);
    Encoder->LiteralProbabilities = NULL;
    Encoder->SaveState.LiteralProbabilities = NULL;
    return Encoder;
}

VOID
LzpLzmaDestroyEncoder (
    PLZMA_ENCODER Encoder,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys and frees an LZMA encoder context.

Arguments:

    Encoder - Supplies a pointer to the encoder to destroy.

    Context - Supplies a pointer to the system context.

Return Value:

    None.

--*/

{

    LzpDestroyMatchFinder(&(Encoder->MatchFinderData), Context);
    LzpLzmaDestroyLiterals(Encoder, Context);
    LzpRangeEncoderDestroy(&(Encoder->RangeEncoder), Context);
    Context->Reallocate(Encoder, 0);
    return;
}

VOID
LzpLzmaDestroyLiterals (
    PLZMA_ENCODER Encoder,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys and frees the literals in an LZMA context.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Context - Supplies a pointer to the system context.

Return Value:

    None.

--*/

{

    Context->Reallocate(Encoder->LiteralProbabilities, 0);
    Encoder->LiteralProbabilities = NULL;
    Context->Reallocate(Encoder->SaveState.LiteralProbabilities, 0);
    Encoder->SaveState.LiteralProbabilities = NULL;
    return;
}

LZ_STATUS
LzpLzmaEncoderSetProperties (
    PLZMA_ENCODER Encoder,
    PLZMA_ENCODER_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine sets the properties of the given LZMA encoder.

Arguments:

    Encoder - Supplies a pointer to the encoder whose properties should be set.

    Properties - Supplies a pointer to the properties to set. A copy of this
        memory will be made.

Return Value:

    LZ Status.

--*/

{

    ULONG FastBytes;
    ULONG HashBytes;
    LZMA_ENCODER_PROPERTIES NewProperties;

    memcpy(&NewProperties, Properties, sizeof(LZMA_ENCODER_PROPERTIES));
    LzpLzmaNormalizeProperties(&NewProperties);
    if ((NewProperties.Lc > LZMA_LC_MAX) ||
        (NewProperties.Lp > LZMA_LP_MAX) ||
        (NewProperties.Pb > LZMA_PB_MAX) ||
        (NewProperties.DictionarySize > (1ULL << LZMA_DICT_LOG_MAX_COMPRESS)) ||
        (NewProperties.DictionarySize > LZMA_MAX_HISTORY_SIZE)) {

        return LzErrorInvalidParameter;
    }

    Encoder->DictSize = NewProperties.DictionarySize;
    FastBytes = NewProperties.FastBytes;
    if (FastBytes < 5) {
        FastBytes = 5;

    } else if (FastBytes > LZMA_MAX_MATCH_LENGTH) {
        FastBytes = LZMA_MAX_MATCH_LENGTH;
    }

    Encoder->FastByteCount = FastBytes;
    Encoder->Lc = NewProperties.Lc;
    Encoder->Lp = NewProperties.Lp;
    Encoder->Pb = NewProperties.Pb;
    Encoder->FastMode = FALSE;
    if (NewProperties.Algorithm == 0) {
        Encoder->FastMode = TRUE;
    }

    HashBytes = 4;
    Encoder->MatchFinderData.BinTreeMode = FALSE;
    if (NewProperties.BinTreeMode != FALSE) {
        Encoder->MatchFinderData.BinTreeMode = TRUE;
        if (NewProperties.HashByteCount < 2) {
            HashBytes = 2;

        } else if (NewProperties.HashByteCount < 4) {
            HashBytes = NewProperties.HashByteCount;
        }
    }

    Encoder->MatchFinderData.HashByteCount = HashBytes;
    Encoder->MatchFinderData.CutValue = NewProperties.MatchCount;
    Encoder->WriteEndMark = NewProperties.EndMark;
    Encoder->Multithread = FALSE;
    if (NewProperties.ThreadCount > 1) {
        Encoder->Multithread = TRUE;
    }

    return LzSuccess;
}

LZ_STATUS
LzpLzmaWriteProperties (
    PLZMA_ENCODER Encoder,
    PUCHAR Properties,
    PUINTN PropertiesSize
    )

/*++

Routine Description:

    This routine writes the properties of the encoder out to a binary byte
    stream.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Properties - Supplies a pointer where the encoded properties will be
        returned on success.

    PropertiesSize - Supplies a pointer that on input contains the size
        of the encoded properties buffer. On output, contains the size of the
        encoded properties.

Return Value:

    LZ status.

--*/

{

    ULONG Bit;
    ULONG DictMask;
    ULONG DictSize;
    ULONG Index;

    DictSize = Encoder->DictSize;
    if (*PropertiesSize < LZMA_PROPERTIES_SIZE) {
        return LzErrorInvalidParameter;
    }

    *PropertiesSize = LZMA_PROPERTIES_SIZE;
    Properties[0] =
                (UCHAR)((((Encoder->Pb * 5) + Encoder->Lp) * 9) + Encoder->Lc);

    if (DictSize >= (1 << 22)) {
        DictMask = (1 << 20) - 1;
        if (DictSize < 0xFFFFFFFF - DictMask) {
            DictSize = (DictSize + DictMask) & ~DictMask;
        }

    } else {
        for (Bit = 11; Bit <= 30; Bit += 1) {
            if (DictSize <= (2 << Bit)) {
                DictSize = 2 << Bit;
                break;
            }

            if (DictSize <= (3 << Bit)) {
                DictSize = 3 << Bit;
                break;
            }
        }
    }

    for (Index = 0; Index < 4; Index += 1) {
        Properties[1 + Index] = (UCHAR)(DictSize >> (8 * Index));
    }

    return LzSuccess;
}

VOID
LzpLzmaNormalizeProperties (
    PLZMA_ENCODER_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine normalizes LZMA properties, getting them in range.

Arguments:

    Properties - Supplies a pointer to the properties.

Return Value:

    None.

--*/

{

    ULONG Index;
    INT Level;

    Level = Properties->Level;
    if (Level < 0) {
        Level = 5;
    }

    Properties->Level = Level;
    if (Properties->DictionarySize == 0) {
        if (Level <= 5) {
            Properties->DictionarySize = 1 << ((Level * 2) + 14);

        } else if (Level == 6) {
            Properties->DictionarySize = 1 << 25;

        } else {
            Properties->DictionarySize = 1 << 26;
        }
    }

    if (Properties->DictionarySize > Properties->ReduceSize) {
        for (Index = 11; Index <= 30; Index += 1) {
            if ((ULONG)(Properties->ReduceSize) <= (2 << Index)) {
                Properties->DictionarySize = 2 << Index;
                break;
            }

            if ((ULONG)(Properties->ReduceSize) <= (3 << Index)) {
                Properties->DictionarySize = 3 << Index;
                break;
            }
        }
    }

    if (Properties->Lc < 0) {
        Properties->Lc = 3;
    }

    if (Properties->Lp < 0) {
        Properties->Lp = 0;
    }

    if (Properties->Pb < 0) {
        Properties->Pb = 2;
    }

    if (Properties->Algorithm < 0) {
        Properties->Algorithm = 0;
        if (Level >= 5) {
            Properties->Algorithm = 1;
        }
    }

    if (Properties->FastBytes < 0) {
        Properties->FastBytes = 32;
        if (Level >= 7) {
            Properties->FastBytes = 64;
        }
    }

    if (Properties->BinTreeMode < 0) {
        Properties->BinTreeMode = FALSE;
        if (Properties->Algorithm != 0) {
            Properties->BinTreeMode = TRUE;
        }
    }

    if (Properties->HashByteCount < 0) {
        Properties->HashByteCount = 4;
    }

    if (Properties->MatchCount == 0) {
        Properties->MatchCount = (16 + (Properties->FastBytes >> 1));
        if (Properties->BinTreeMode == FALSE) {
            Properties->MatchCount >>= 1;
        }
    }

    if (Properties->ThreadCount < 0) {
        Properties->ThreadCount = 1;
    }

    return;
}

VOID
LzpLzmaInitializeFastPosition (
    PUCHAR FastPosition
    )

/*++

Routine Description:

    This routine initializes the fast position table for an LZMA encoder.

Arguments:

    FastPosition - Supplies a pointer to the fast position table.

Return Value:

    None.

--*/

{

    UINTN Count;
    UINTN Index;
    ULONG Slot;

    FastPosition[0] = 0;
    FastPosition[1] = 1;
    FastPosition += 2;
    for (Slot = 2; Slot < (LZMA_DICT_LOG_BITS * 2); Slot += 1) {
        Count = 1UL << ((Slot >> 1) - 1);
        for (Index = 0; Index < Count; Index += 1) {
            FastPosition[Index] = Slot;
        }

        FastPosition += Count;
    }

    return;
}

VOID
LzpLzmaInitializePriceTables (
    PULONG Prices
    )

/*++

Routine Description:

    This routine initializes the price table for an LZMA encoder.

Arguments:

    Prices - Supplies a pointer to the probability prices.

Return Value:

    None.

--*/

{

    ULONG Bit;
    ULONG BitCount;
    ULONG Index;
    ULONG Weight;

    for (Index = (1 << LZMA_MOVE_REDUCING_BITS) / 2;
         Index < LZMA_BIT_MODEL_TOTAL;
         Index += (1 << LZMA_MOVE_REDUCING_BITS)) {

        Weight = Index;
        BitCount = 0;
        for (Bit = 0; Bit < LZMA_BIT_PRICE_SHIFT_BITS; Bit += 1) {
            Weight = Weight * Weight;
            BitCount <<= 1;
            while (Weight >= (ULONG)(1 << 16)) {
                Weight >>= 1;
                BitCount += 1;
            }
        }

        Prices[Index >> LZMA_MOVE_REDUCING_BITS] =
            (LZMA_BIT_MODEL_BIT_COUNT << LZMA_BIT_PRICE_SHIFT_BITS) - 15 -
            BitCount;
    }

    return;
}

LZ_STATUS
LzpLzmaAllocateBuffers (
    PLZMA_ENCODER Encoder,
    ULONG KeepWindowSize,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine allocates encoder buffers in preparation for LZMA encoding.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    KeepWindowSize - Supplies the minimum window size to use.

    Context - Supplies a pointer to the general LZ context.

Return Value:

    LZ status.

--*/

{

    ULONG AdvanceSize;
    UINTN AllocationSize;
    ULONG BeforeSize;
    ULONG Log;
    LZ_STATUS Result;
    ULONG Sum;

    for (Log = 0; Log < LZMA_DICT_LOG_MAX_COMPRESS; Log += 1) {
        if (Encoder->DictSize <= (1 << Log)) {
            break;
        }
    }

    Encoder->DistanceTableSize = Log * 2;
    Encoder->Finished = FALSE;
    Encoder->Result = LzSuccess;

    //
    // Allocate encoder buffers.
    //

    Result = LzpRangeEncoderInitialize(&(Encoder->RangeEncoder), Context);
    if (Result != LzSuccess) {
        return Result;
    }

    Sum = Encoder->Lc + Encoder->Lp;
    if ((Encoder->LiteralProbabilities == NULL) ||
        (Encoder->SaveState.LiteralProbabilities == NULL) ||
        (Encoder->LcLp != Sum)) {

        LzpLzmaDestroyLiterals(Encoder, Context);
        AllocationSize = (0x300 << Sum) * sizeof(LZ_PROB);
        Encoder->LiteralProbabilities =
                                     Context->Reallocate(NULL, AllocationSize);

        Encoder->SaveState.LiteralProbabilities =
                                     Context->Reallocate(NULL, AllocationSize);

        if ((Encoder->LiteralProbabilities == NULL) ||
            (Encoder->SaveState.LiteralProbabilities == NULL)) {

            LzpLzmaDestroyLiterals(Encoder, Context);
            return LzErrorMemory;
        }

        Encoder->LcLp = Sum;
    }

    Encoder->MatchFinderData.BigHash = FALSE;
    if (Encoder->DictSize > LZMA_BIG_HASH_DICT_LIMIT) {
        Encoder->MatchFinderData.BigHash = TRUE;
    }

    BeforeSize = LZMA_OPTIMAL_COUNT;
    if (BeforeSize + Encoder->DictSize < KeepWindowSize) {
        BeforeSize = KeepWindowSize - Encoder->DictSize;
    }

    //
    // The number of bytes of data ahead to keep is bounded by the maximum
    // number of times get matches will be called between emitting symbols. At
    // worst case it may be called once for each potential price, and have a
    // huge match at the end.
    //

    AdvanceSize = LZMA_OPTIMAL_COUNT + LZMA_MAX_MATCH_LENGTH;
    Result = LzpMatchFinderAllocateBuffers(&(Encoder->MatchFinderData),
                                           Encoder->DictSize,
                                           BeforeSize,
                                           Encoder->FastByteCount,
                                           AdvanceSize,
                                           Context);

    if (Result != LzSuccess) {
        return Result;
    }

    Encoder->MatchFinderContext = &(Encoder->MatchFinderData);
    LzpMatchFinderInitializeInterface(&(Encoder->MatchFinderData),
                                      &(Encoder->MatchFinder));

    //
    // Reset the encoder for a fresh run.
    //

    LzpLzmaResetEncoder(Encoder);
    LzpLzmaResetPrices(Encoder);
    Context->UncompressedSize = 0;
    return LzSuccess;
}

VOID
LzpLzmaResetEncoder (
    PLZMA_ENCODER Encoder
    )

/*++

Routine Description:

    This routine initializes and resets the encoder for a fresh block.

Arguments:

    Encoder - Supplies a pointer to the encoder.

Return Value:

    None.

--*/

{

    ULONG Count;
    ULONG Index;
    ULONG PbState;
    PLZ_PROB Probability;
    ULONG SlotIndex;

    for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
        Encoder->Reps[Index] = 0;
    }

    LzpRangeEncoderReset(&(Encoder->RangeEncoder));
    for (Index = 0; Index < LZMA_STATE_COUNT; Index += 1) {
        for (PbState = 0; PbState < LZMA_MAX_PB_STATES; PbState += 1) {
            Encoder->IsMatch[Index][PbState] = LZMA_INITIAL_PROBABILITY;
            Encoder->IsRep0Long[Index][PbState] = LZMA_INITIAL_PROBABILITY;
        }

        Encoder->IsRep[Index] = LZMA_INITIAL_PROBABILITY;
        Encoder->IsRepG0[Index] = LZMA_INITIAL_PROBABILITY;
        Encoder->IsRepG1[Index] = LZMA_INITIAL_PROBABILITY;
        Encoder->IsRepG2[Index] = LZMA_INITIAL_PROBABILITY;
    }

    Count = 0x300 << Encoder->LcLp;
    for (Index = 0; Index < Count; Index += 1) {
        Encoder->LiteralProbabilities[Index] = LZMA_INITIAL_PROBABILITY;
    }

    for (Index = 0; Index < LZMA_LENGTH_TO_POSITION_STATES; Index += 1) {
        Probability = Encoder->SlotEncoder[Index];
        for (SlotIndex = 0;
             SlotIndex < (1 << LZMA_POSITION_SLOT_BITS);
             SlotIndex += 1) {

            Probability[SlotIndex] = LZMA_INITIAL_PROBABILITY;
        }
    }

    for (Index = 0;
         Index < LZMA_FULL_DISTANCES - LZMA_END_POSITION_MODEL_INDEX;
         Index += 1) {

        Encoder->Encoders[Index] = LZMA_INITIAL_PROBABILITY;
    }

    LzpLengthEncoderInitialize(&(Encoder->LengthEncoder.LengthEncoder));
    LzpLengthEncoderInitialize(&(Encoder->RepLengthEncoder.LengthEncoder));
    for (Index = 0; Index < (1 << LZMA_ALIGN_TABLE_BITS); Index += 1) {
        Encoder->AlignEncoder[Index] = LZMA_INITIAL_PROBABILITY;
    }

    Encoder->Processed = 0;
    Encoder->OptimumEndIndex = 0;
    Encoder->OptimumCurrentIndex = 0;
    Encoder->AdditionalOffset = 0;
    Encoder->PbMask = (1 << Encoder->Pb) - 1;
    Encoder->LpMask = (1 << Encoder->Lp) - 1;
    return;
}

VOID
LzpLzmaResetPrices (
    PLZMA_ENCODER Encoder
    )

/*++

Routine Description:

    This routine resets the price tables.

Arguments:

    Encoder - Supplies a pointer to the encoder.

Return Value:

    None.

--*/

{

    if (Encoder->FastMode == FALSE) {
        LzpLzmaFillDistancesPrices(Encoder);
        LzpLzmaFillAlignPrices(Encoder);
    }

    Encoder->LengthEncoder.TableSize =
                            Encoder->FastByteCount + 1 - LZMA_MIN_MATCH_LENGTH;

    Encoder->RepLengthEncoder.TableSize = Encoder->LengthEncoder.TableSize;
    LzpLengthPriceEncoderUpdateTables(&(Encoder->LengthEncoder),
                                      1 << Encoder->Pb,
                                      Encoder->ProbabilityPrices);

    LzpLengthPriceEncoderUpdateTables(&(Encoder->RepLengthEncoder),
                                      1 << Encoder->Pb,
                                      Encoder->ProbabilityPrices);

    return;
}

VOID
LzpLzmaFillDistancesPrices (
    PLZMA_ENCODER Encoder
    )

/*++

Routine Description:

    This routine initializes the distances prices.

Arguments:

    Encoder - Supplies a pointer to the encoder.

Return Value:

    None.

--*/

{

    ULONG Base;
    PULONG DistancesPrices;
    ULONG FooterBits;
    ULONG Index;
    ULONG LengthToPositionState;
    ULONG PositionSlot;
    PULONG PositionSlotPrices;
    PLZ_PROB SlotEncoder;
    ULONG WorkingPrices[LZMA_FULL_DISTANCES];

    for (Index = LZMA_START_POSITION_MODEL_INDEX;
         Index < LZMA_FULL_DISTANCES;
         Index += 1) {

        PositionSlot = LzpLzmaGetPositionSlot(Encoder, Index);
        FooterBits = (PositionSlot >> 1) - 1;
        Base = (2 | (PositionSlot & 0x1)) << FooterBits;
        WorkingPrices[Index] = LzpRcTreeReverseGetPrice(
                                   Encoder->Encoders + Base - PositionSlot - 1,
                                   FooterBits,
                                   Index - Base,
                                   Encoder->ProbabilityPrices);
    }

    for (LengthToPositionState = 0;
         LengthToPositionState < LZMA_LENGTH_TO_POSITION_STATES;
         LengthToPositionState += 1) {

        SlotEncoder = Encoder->SlotEncoder[LengthToPositionState];
        PositionSlotPrices = Encoder->SlotPrices[LengthToPositionState];
        for (PositionSlot = 0;
             PositionSlot < Encoder->DistanceTableSize;
             PositionSlot += 1) {

            PositionSlotPrices[PositionSlot] = LzpRcTreeGetPrice(
                                                   SlotEncoder,
                                                   LZMA_POSITION_SLOT_BITS,
                                                   PositionSlot,
                                                   Encoder->ProbabilityPrices);
        }

        for (PositionSlot = LZMA_END_POSITION_MODEL_INDEX;
             PositionSlot < Encoder->DistanceTableSize;
             PositionSlot += 1) {

            PositionSlotPrices[PositionSlot] +=
                         (((PositionSlot >> 1) - 1) - LZMA_ALIGN_TABLE_BITS) <<
                         LZMA_BIT_PRICE_SHIFT_BITS;
        }

        DistancesPrices = Encoder->DistancesPrices[LengthToPositionState];
        for (Index = 0; Index < LZMA_START_POSITION_MODEL_INDEX; Index += 1) {
            DistancesPrices[Index] = PositionSlotPrices[Index];
        }

        while (Index < LZMA_FULL_DISTANCES) {
            PositionSlot = LzpLzmaGetPositionSlot(Encoder, Index);
            DistancesPrices[Index] = PositionSlotPrices[PositionSlot] +
                                     WorkingPrices[Index];

            Index += 1;
        }
    }

    Encoder->MatchPriceCount = 0;
    return;
}

VOID
LzpLzmaFillAlignPrices (
    PLZMA_ENCODER Encoder
    )

/*++

Routine Description:

    This routine initializes the align prices.

Arguments:

    Encoder - Supplies a pointer to the encoder.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < LZMA_ALIGN_TABLE_SIZE; Index += 1) {
        Encoder->AlignPrices[Index] = LzpRcTreeReverseGetPrice(
                                                   Encoder->AlignEncoder,
                                                   LZMA_ALIGN_TABLE_BITS,
                                                   Index,
                                                   Encoder->ProbabilityPrices);
    }

    Encoder->AlignPriceCount = 0;
    return;
}

LZ_STATUS
LzpLzmaEncode (
    PLZMA_ENCODER Encoder,
    BOOL UseLimits,
    ULONG MaxPackSize,
    ULONG MaxUnpackSize,
    LZ_FLUSH_OPTION Flush
    )

/*++

Routine Description:

    This routine implements the crux of the LZMA encoder, the encoding of a
    block of data.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    UseLimits - Supplies a boolean indicating whether the next two parameters
        are valid and should be honored.

    MaxPackSize - Supplies the maximum encoded block size.

    MaxUnpackSize - Supplies the maximum decoded block size.

    Flush - Supplies the flush option, used to decide how to buffer input
        data.

Return Value:

    LZ Status.

--*/

{

    ULONG Base;
    UCHAR Byte;
    ULONG CurrentPosition32;
    PCUCHAR Data;
    ULONG Distance;
    ULONG FooterBits;
    ULONG Length;
    ULONG PairCount;
    ULONG Position;
    ULONG PositionReduced;
    ULONG PositionSlot;
    ULONG PositionState;
    PLZ_PROB Probabilities;
    ULONG Processed;
    PLZMA_RANGE_ENCODER Range;
    LZ_STATUS Result;
    ULONG StartPosition32;

    if (Encoder->NeedInitialization != FALSE) {
        Encoder->MatchFinder.Initialize(Encoder->MatchFinderContext);
        Encoder->NeedInitialization = FALSE;
    }

    if (Encoder->Finished != FALSE) {
        return Encoder->Result;
    }

    Result = LzpLzmaEncoderGetError(Encoder);
    if (Result != LzSuccess) {
        return Result;
    }

    //
    // Ask the match finder to load up on input. If the user is supplying tiny
    // amounts of input at a time, this makes sure that progress is being made.
    //

    Result = Encoder->MatchFinder.Load(Encoder->MatchFinderContext, Flush);
    if (Result != LzSuccess) {
        return Result;
    }

    Range = &(Encoder->RangeEncoder);
    CurrentPosition32 = (ULONG)(Encoder->Processed);
    StartPosition32 = CurrentPosition32;

    //
    // Handle the very beginning of the stream. Watch out for an empty
    // stream, otherwise emit the first literal since there's nothing to
    // repeat.
    //

    if (Encoder->Processed == 0) {
        if (Encoder->MatchFinder.GetCount(Encoder->MatchFinderContext) == 0) {
            goto LzmaEncodeEnd;
        }

        LzpLzmaReadMatchDistances(Encoder, &PairCount);

        //
        // Emit a literal: 0 bit plus the byte.
        //

        LzpRangeEncodeBit(Range, &(Encoder->IsMatch[Encoder->State][0]), 0);
        Encoder->State = LzLzmaLiteralNextStates[Encoder->State];
        Data = Encoder->MatchFinder.GetPosition(Encoder->MatchFinderContext) -
               Encoder->AdditionalOffset;

        Byte = *Data;
        LzpLiteralEncoderEncode(Range, Encoder->LiteralProbabilities, Byte);
        Encoder->AdditionalOffset -= 1;
        CurrentPosition32 += 1;
    }

    if (Encoder->MatchFinder.GetCount(Encoder->MatchFinderContext) == 0) {
        goto LzmaEncodeEnd;
    }

    //
    // Loop encoding symbols as long as there's both input and output space.
    //

    while (LZMA_HAS_BUFFER_SPACE(Encoder, Flush)) {

        //
        // Go do all the work to figure out the longest match.
        //

        if (Encoder->FastMode != FALSE) {
            Length = LzpLzmaGetOptimumFast(Encoder,
                                           CurrentPosition32,
                                           &Position);

        } else {
            Length = LzpLzmaGetOptimum(Encoder,
                                       CurrentPosition32,
                                       &Position);
        }

        PositionState = CurrentPosition32 & Encoder->PbMask;

        //
        // If there's no repeat match, emit a literal, which is a zero bit
        // plus the byte.
        //

        if ((Length == 1) && (Position == (ULONG)-1)) {
            LzpRangeEncodeBit(
                            Range,
                            &(Encoder->IsMatch[Encoder->State][PositionState]),
                            0);

            Data = Encoder->MatchFinder.GetPosition(
                                                Encoder->MatchFinderContext);

            Data -= Encoder->AdditionalOffset;
            Byte = *Data;
            Probabilities = LZP_LITERAL_PROBABILITIES(Encoder,
                                                      CurrentPosition32,
                                                      *(Data - 1));

            if (LZP_IS_CHARACTER_STATE(Encoder->State)) {
                LzpLiteralEncoderEncode(Range, Probabilities, Byte);

            } else {
                LzpLiteralEncoderEncodeMatched(Range,
                                               Probabilities,
                                               Byte,
                                               *(Data - Encoder->Reps[0] - 1));
            }

            Encoder->State = LzLzmaLiteralNextStates[Encoder->State];

        //
        // There's a match or repeat of some kind. Emit a 1 bit.
        //

        } else {
            LzpRangeEncodeBit(
                            Range,
                            &(Encoder->IsMatch[Encoder->State][PositionState]),
                            1);

            //
            // If it's a rep of some kind, emit another 1.
            //

            if (Position < LZMA_REP_COUNT) {
                LzpRangeEncodeBit(Range, &(Encoder->IsRep[Encoder->State]), 1);

                //
                // If the position is the last used distance, then it's either
                // a shortrep or a longrep[0]. A shortrep is length 1, so
                // emit bits 1100. A longrep contains a length, so emit
                // 1101, then the length (a bit further down).
                //

                if (Position == 0) {
                    LzpRangeEncodeBit(Range,
                                      &(Encoder->IsRepG0[Encoder->State]),
                                      0);

                    LzpRangeEncodeBit(
                         Range,
                         &(Encoder->IsRep0Long[Encoder->State][PositionState]),
                         Length != 1);

                //
                // This is a long rep, using one of the last 4 distances used.
                // Emit 1110 for longrep[1] (second to last distance used),
                // 11110 for longrep[2], and 11111 for longrep[3] (fourth to
                // last used distance).
                //

                } else {
                    Distance = Encoder->Reps[Position];
                    LzpRangeEncodeBit(Range,
                                      &(Encoder->IsRepG0[Encoder->State]),
                                      1);

                    if (Position == 1) {
                        LzpRangeEncodeBit(Range,
                                          &(Encoder->IsRepG1[Encoder->State]),
                                          0);

                    } else {
                        LzpRangeEncodeBit(Range,
                                          &(Encoder->IsRepG1[Encoder->State]),
                                          1);

                        LzpRangeEncodeBit(Range,
                                          &(Encoder->IsRepG2[Encoder->State]),
                                          Position - 2);

                        if (Position == 3) {
                            Encoder->Reps[3] = Encoder->Reps[2];
                        }

                        Encoder->Reps[2] = Encoder->Reps[1];
                    }

                    //
                    // Reinsert the rep used at the front.
                    //

                    Encoder->Reps[1] = Encoder->Reps[0];
                    Encoder->Reps[0] = Distance;
                }

                //
                // If there's a length (all except shortrep), emit the length.
                //

                if (Length == 1) {
                    Encoder->State = LzLzmaShortRepNextStates[Encoder->State];

                } else {
                    LzpLengthEncoderEncodeAndUpdate(
                                                &(Encoder->RepLengthEncoder),
                                                Range,
                                                Length - LZMA_MIN_MATCH_LENGTH,
                                                PositionState,
                                                !Encoder->FastMode,
                                                Encoder->ProbabilityPrices);

                    Encoder->State = LzLzmaRepNextStates[Encoder->State];
                }

            //
            // Emit a match: bit 1 was already emitted, then 0, then sequence
            // length and sitance.
            //

            } else {
                LzpRangeEncodeBit(Range,
                                  &(Encoder->IsRep[Encoder->State]),
                                  0);

                Encoder->State = LzLzmaMatchNextStates[Encoder->State];
                LzpLengthEncoderEncodeAndUpdate(&(Encoder->LengthEncoder),
                                                Range,
                                                Length - LZMA_MIN_MATCH_LENGTH,
                                                PositionState,
                                                !Encoder->FastMode,
                                                Encoder->ProbabilityPrices);

                Position -= LZMA_REP_COUNT;
                PositionSlot = LzpLzmaGetPositionSlot(Encoder, Position);
                PositionState = LZP_GET_LENGTH_TO_POSITION_STATE(Length);
                LzpRcTreeEncode(Range,
                                Encoder->SlotEncoder[PositionState],
                                LZMA_POSITION_SLOT_BITS,
                                PositionSlot);

                if (PositionSlot >= LZMA_START_POSITION_MODEL_INDEX) {
                    FooterBits = (PositionSlot >> 1) - 1;
                    Base = (2 | (PositionSlot & 0x1)) << FooterBits;
                    PositionReduced = Position - Base;
                    if (PositionSlot < LZMA_END_POSITION_MODEL_INDEX) {
                        LzpRcTreeReverseEncode(
                                   Range,
                                   Encoder->Encoders + Base - PositionSlot - 1,
                                   FooterBits,
                                   PositionReduced);

                    } else {
                        LzpRangeEncodeDirectBits(
                                      Range,
                                      PositionReduced >> LZMA_ALIGN_TABLE_BITS,
                                      FooterBits - LZMA_ALIGN_TABLE_BITS);

                        LzpRcTreeReverseEncode(
                                            Range,
                                            Encoder->AlignEncoder,
                                            LZMA_ALIGN_TABLE_BITS,
                                            PositionReduced & LZMA_ALIGN_MASK);

                        Encoder->AlignPriceCount += 1;
                    }
                }

                Encoder->Reps[3] = Encoder->Reps[2];
                Encoder->Reps[2] = Encoder->Reps[1];
                Encoder->Reps[1] = Encoder->Reps[0];
                Encoder->Reps[0] = Position;
                Encoder->MatchPriceCount += 1;
            }
        }

        Encoder->AdditionalOffset -= Length;
        CurrentPosition32 += Length;
        if (Encoder->AdditionalOffset == 0) {
            if (Encoder->FastMode == FALSE) {
                if (Encoder->MatchPriceCount >= (1 << 7)) {
                    LzpLzmaFillDistancesPrices(Encoder);
                }

                if (Encoder->AlignPriceCount >= LZMA_ALIGN_TABLE_SIZE) {
                    LzpLzmaFillAlignPrices(Encoder);
                }
            }

            if (Encoder->MatchFinder.GetCount(Encoder->MatchFinderContext) ==
                0) {

                break;
            }

            Processed = CurrentPosition32 - StartPosition32;
            if (UseLimits != FALSE) {
                if ((Processed + LZMA_OPTIMAL_COUNT + 300 >= MaxUnpackSize) ||
                    (LzpRangeEncoderGetProcessed(Range) +
                     (LZMA_OPTIMAL_COUNT * 2) >= MaxPackSize)) {

                    break;
                }

            //
            // Update the uncompressed size occasionally in the loop, otherwise
            // chunks of 4GB could be lost.
            //

            } else if (Processed >= (1 << 24)) {
                Encoder->Processed += Processed;
                StartPosition32 = CurrentPosition32;
            }
        }
    }

LzmaEncodeEnd:
    Encoder->Processed += CurrentPosition32 - StartPosition32;
    return LzpLzmaEncoderGetError(Encoder);
}

LZ_STATUS
LzpLzmaEncoderFlush (
    PLZMA_ENCODER Encoder,
    ULONG Position
    )

/*++

Routine Description:

    This routine flushes the current encoded data from the encoder.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Position - Supplies the current 32-bit position.

Return Value:

    LZ status.

--*/

{

    Encoder->Finished = TRUE;
    if (Encoder->WriteEndMark != FALSE) {
        LzpLzmaEncoderWriteEndMark(Encoder, Position & Encoder->PbMask);
    }

    LzpRangeEncoderFlushData(&(Encoder->RangeEncoder));
    LzpRangeEncoderFlushStream(&(Encoder->RangeEncoder));
    return LzpLzmaEncoderGetError(Encoder);
}

VOID
LzpLzmaEncoderWriteEndMark (
    PLZMA_ENCODER Encoder,
    ULONG PositionState
    )

/*++

Routine Description:

    This routine writes the end marker to the output stream.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    PositionState - Supplies the current position state.

Return Value:

    None.

--*/

{

    ULONG Length;
    PLZMA_RANGE_ENCODER Range;

    Range = &(Encoder->RangeEncoder);
    LzpRangeEncodeBit(Range,
                      &(Encoder->IsMatch[Encoder->State][PositionState]),
                      1);

    LzpRangeEncodeBit(Range, &(Encoder->IsRep[Encoder->State]), 0);
    Encoder->State = LzLzmaMatchNextStates[Encoder->State];
    Length = LZMA_MIN_MATCH_LENGTH;
    LzpLengthEncoderEncodeAndUpdate(&(Encoder->LengthEncoder),
                                    Range,
                                    Length - LZMA_MIN_MATCH_LENGTH,
                                    PositionState,
                                    !Encoder->FastMode,
                                    Encoder->ProbabilityPrices);

    PositionState = LZP_GET_LENGTH_TO_POSITION_STATE(Length);
    LzpRcTreeEncode(Range,
                    Encoder->SlotEncoder[PositionState],
                    LZMA_POSITION_SLOT_BITS,
                    (1 << LZMA_POSITION_SLOT_BITS) - 1);

    LzpRangeEncodeDirectBits(Range,
                             ((1UL << 30) - 1) >> LZMA_ALIGN_TABLE_BITS,
                             30 - LZMA_ALIGN_TABLE_BITS);

    LzpRcTreeReverseEncode(Range,
                           Encoder->AlignEncoder,
                           LZMA_ALIGN_TABLE_BITS,
                           LZMA_ALIGN_MASK);

    return;
}

LZ_STATUS
LzpLzmaEncoderGetError (
    PLZMA_ENCODER Encoder
    )

/*++

Routine Description:

    This routine returns the first error that's set in any of the encoder
    machines.

Arguments:

    Encoder - Supplies a pointer to the encoder.

Return Value:

    LZ error status from any of the encoder submachines.

    LzSuccess if there is no error.

--*/

{

    if (Encoder->Result != LzSuccess) {
        return Encoder->Result;
    }

    if (Encoder->RangeEncoder.Result != LzSuccess) {
        Encoder->Result = LzErrorWrite;

    } else if (Encoder->MatchFinderData.Result != LzSuccess) {
        Encoder->Result = LzErrorRead;
    }

    if (Encoder->Result != LzSuccess) {
        Encoder->Finished = TRUE;
    }

    return Encoder->Result;
}

VOID
LzpLengthEncoderEncodeAndUpdate (
    PLZMA_LENGTH_PRICE_ENCODER LengthPrice,
    PLZMA_RANGE_ENCODER Range,
    ULONG Symbol,
    ULONG PositionState,
    BOOL UpdatePrice,
    PULONG ProbabilityPrices
    )

/*++

Routine Description:

    This routine emits the encoded length value and potentially updates the
    length price table.

Arguments:

    LengthPrice - Supplies a pointer to the length/price encoder.

    Range - Supplies a pointer to the range encoder.

    Symbol - Supplies the symbol to encode.

    PositionState - Supplies the current position state.

    UpdatePrice - Supplies a boolean indicating whether or not to update the
        price. This is generally FALSE during fast mode (when no prices are
        computed) or TRUE during normal mode.

    ProbabilityPrices - Supplies a pointer to an array of probability prices.

Return Value:

    None.

--*/

{

    LzpLengthEncoderEncode(&(LengthPrice->LengthEncoder),
                           Range,
                           Symbol,
                           PositionState);

    if (UpdatePrice != FALSE) {
        LengthPrice->Counters[PositionState] -= 1;
        if (LengthPrice->Counters[PositionState] == 0) {
            LzpLengthPriceEncoderUpdateTable(LengthPrice,
                                             PositionState,
                                             ProbabilityPrices);
        }
    }

    return;
}

VOID
LzpLengthPriceEncoderUpdateTables (
    PLZMA_LENGTH_PRICE_ENCODER LengthPrice,
    ULONG PositionStateCount,
    PULONG ProbabilityPrices
    )

/*++

Routine Description:

    This routine updates each of the length price encoder tables.

Arguments:

    LengthPrice - Supplies a pointer to the length/price encoder.

    PositionStateCount - Supplies the number of position states.

    ProbabilityPrices - Supplies a pointer to an array of probability prices.

Return Value:

    None.

--*/

{

    ULONG PositionState;

    for (PositionState = 0;
         PositionState < PositionStateCount;
         PositionState += 1) {

        LzpLengthPriceEncoderUpdateTable(LengthPrice,
                                         PositionState,
                                         ProbabilityPrices);
    }

    return;
}

VOID
LzpLengthPriceEncoderUpdateTable (
    PLZMA_LENGTH_PRICE_ENCODER LengthPrice,
    ULONG PositionState,
    PULONG ProbabilityPrices
    )

/*++

Routine Description:

    This routine updates one of the length price encoder tables.

Arguments:

    LengthPrice - Supplies a pointer to the length/price encoder.

    PositionState - Supplies the index of the table to update.

    ProbabilityPrices - Supplies a pointer to an array of probability prices.

Return Value:

    None.

--*/

{

    LzpLengthEncoderSetPrices(&(LengthPrice->LengthEncoder),
                              PositionState,
                              LengthPrice->TableSize,
                              LengthPrice->Prices[PositionState],
                              ProbabilityPrices);

    LengthPrice->Counters[PositionState] = LengthPrice->TableSize;
    return;
}

VOID
LzpLengthEncoderInitialize (
    PLZMA_LENGTH_ENCODER LengthEncoder
    )

/*++

Routine Description:

    This routine resets an LZMA length encoder.

Arguments:

    LengthEncoder - Supplies a pointer to the length encoder.

Return Value:

    None.

--*/

{

    ULONG Index;

    LengthEncoder->Choice = LZMA_INITIAL_PROBABILITY;
    LengthEncoder->Choice2 = LZMA_INITIAL_PROBABILITY;
    for (Index = 0;
         Index < (LZMA_MAX_PB_STATES << LZMA_LENGTH_LOW_BITS);
         Index += 1) {

        LengthEncoder->Low[Index] = LZMA_INITIAL_PROBABILITY;
    }

    for (Index = 0;
         Index < (LZMA_MAX_PB_STATES << LZMA_LENGTH_MID_BITS);
         Index += 1) {

        LengthEncoder->Mid[Index] = LZMA_INITIAL_PROBABILITY;
    }

    for (Index = 0; Index < LZMA_LENGTH_HIGH_SYMBOLS; Index += 1) {
        LengthEncoder->High[Index] = LZMA_INITIAL_PROBABILITY;
    }

    return;
}

VOID
LzpLengthEncoderEncode (
    PLZMA_LENGTH_ENCODER LengthEncoder,
    PLZMA_RANGE_ENCODER RangeEncoder,
    ULONG Symbol,
    ULONG PositionState
    )

/*++

Routine Description:

    This routine emits a symbol to the output stream via the length encoder.

Arguments:

    LengthEncoder - Supplies a pointer to the length encoder.

    RangeEncoder - Supplies a pointer to the output range encoder.

    Symbol - Supplies the symbol to emit.

    PositionState - Supplies the current position state.

Return Value:

    None.

--*/

{

    //
    // If it's smaller than the low cutoff, write out a zero bit then the
    // value.
    //

    if (Symbol < LZMA_LENGTH_LOW_SYMBOLS) {
        LzpRangeEncodeBit(RangeEncoder, &(LengthEncoder->Choice), 0);
        LzpRcTreeEncode(
                  RangeEncoder,
                  LengthEncoder->Low + (PositionState << LZMA_LENGTH_LOW_BITS),
                  LZMA_LENGTH_LOW_BITS,
                  Symbol);

    //
    // Otherwise, if it's smaller than the mid cutoff, write a 10, then the
    // value (minus the low cutoff).
    //

    } else {
        LzpRangeEncodeBit(RangeEncoder, &(LengthEncoder->Choice), 1);
        if (Symbol < LZMA_LENGTH_LOW_SYMBOLS + LZMA_LENGTH_MID_SYMBOLS) {
            LzpRangeEncodeBit(RangeEncoder, &(LengthEncoder->Choice2), 0);
            LzpRcTreeEncode(
                  RangeEncoder,
                  LengthEncoder->Mid + (PositionState << LZMA_LENGTH_MID_BITS),
                  LZMA_LENGTH_MID_BITS,
                  Symbol - LZMA_LENGTH_LOW_SYMBOLS);

        //
        // The value is greater than the mid cutoff, so 11 is written, then the
        // value (minus the mid cutoff).
        //

        } else {
            Symbol -= LZMA_LENGTH_LOW_SYMBOLS + LZMA_LENGTH_MID_SYMBOLS;
            LzpRangeEncodeBit(RangeEncoder, &(LengthEncoder->Choice2), 1);
            LzpRcTreeEncode(RangeEncoder,
                            LengthEncoder->High,
                            LZMA_LENGTH_HIGH_BITS,
                            Symbol);
        }
    }

    return;
}

VOID
LzpLengthEncoderSetPrices (
    PLZMA_LENGTH_ENCODER Length,
    ULONG PositionState,
    ULONG SymbolCount,
    PULONG Prices,
    PULONG ProbabilityPrices
    )

/*++

Routine Description:

    This routine sets the prices in a length encoder.

Arguments:

    Length - Supplies a pointer to the length encoder.

    PositionState - Supplies the index of the table to update.

    SymbolCount - Supplies the number of symbols.

    Prices - Supplies a pointer to the prices to update.

    ProbabilityPrices - Supplies a pointer to the array of probability prices.

Return Value:

    None.

--*/

{

    ULONG A0;
    ULONG A1;
    ULONG B0;
    ULONG B1;
    ULONG Index;

    A0 = ProbabilityPrices[LZP_GET_PRICE_INDEX(Length->Choice, 0)];
    A1 = ProbabilityPrices[LZP_GET_PRICE_INDEX(Length->Choice, 1)];
    B0 = A1 + ProbabilityPrices[LZP_GET_PRICE_INDEX(Length->Choice2, 0)];
    B1 = A1 + ProbabilityPrices[LZP_GET_PRICE_INDEX(Length->Choice2, 1)];
    for (Index = 0; Index < LZMA_LENGTH_LOW_SYMBOLS; Index += 1) {
        if (Index >= SymbolCount) {
            return;
        }

        Prices[Index] = A0;
        Prices[Index] += LzpRcTreeGetPrice(
                          Length->Low + (PositionState << LZMA_LENGTH_LOW_BITS),
                          LZMA_LENGTH_LOW_BITS,
                          Index,
                          ProbabilityPrices);
    }

    while (Index < LZMA_LENGTH_LOW_SYMBOLS + LZMA_LENGTH_MID_SYMBOLS) {
        if (Index >= SymbolCount) {
            return;
        }

        Prices[Index] = B0;
        Prices[Index] += LzpRcTreeGetPrice(
                         Length->Mid + (PositionState << LZMA_LENGTH_MID_BITS),
                         LZMA_LENGTH_MID_BITS,
                         Index - LZMA_LENGTH_LOW_SYMBOLS,
                         ProbabilityPrices);

        Index += 1;
    }

    while (Index < SymbolCount) {
        Prices[Index] = B1;
        Prices[Index] += LzpRcTreeGetPrice(
                     Length->High,
                     LZMA_LENGTH_HIGH_BITS,
                     Index - LZMA_LENGTH_LOW_SYMBOLS - LZMA_LENGTH_MID_SYMBOLS,
                     ProbabilityPrices);

        Index += 1;
    }

    return;
}

VOID
LzpRcTreeEncode (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol
    )

/*++

Routine Description:

    This routine encodes a symbol into the RC tree.

Arguments:

    RangeEncoder - Supplies a pointer to the output range encoder.

    Probabilities - Supplies a pointer to the probabilities.

    BitLevelCount - Supplies the number of bit levels.

    Symbol - Supplies the symbol to encode.

Return Value:

    None.

--*/

{

    ULONG Bit;
    INT Index;
    ULONG Value;

    Index = BitLevelCount;
    Value = 1;
    while (Index != 0) {
        Index -= 1;
        Bit = (Symbol >> Index) & 0x1;
        LzpRangeEncodeBit(RangeEncoder, Probabilities + Value, Bit);
        Value = (Value << 1) | Bit;
    }

    return;
}

VOID
LzpRcTreeReverseEncode (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol
    )

/*++

Routine Description:

    This routine reverse encodes a symbol into the RC tree (from least
    significant bit to greatest).

Arguments:

    RangeEncoder - Supplies a pointer to the output range encoder.

    Probabilities - Supplies a pointer to the probabilities.

    BitLevelCount - Supplies the number of bit levels.

    Symbol - Supplies the symbol to encode.

Return Value:

    None.

--*/

{

    ULONG Bit;
    INT Index;
    ULONG Value;

    Value = 1;
    for (Index = 0; Index < BitLevelCount; Index += 1) {
        Bit = Symbol & 0x1;
        LzpRangeEncodeBit(RangeEncoder, Probabilities + Value, Bit);
        Value = (Value << 1) | Bit;
        Symbol >>= 1;
    }

    return;
}

ULONG
LzpRcTreeGetPrice (
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol,
    PULONG Prices
    )

/*++

Routine Description:

    This routine gets the price for a given symbol.

Arguments:

    Probabilities - Supplies a pointer to the probabilities.

    BitLevelCount - Supplies the count of bit levels.

    Symbol - Supplies the symbol to get the price for.

    Prices - Supplies the array of prices.

Return Value:

    Returns the price for the given symbol.

--*/

{

    ULONG Price;
    UINTN PriceIndex;

    Price = 0;
    Symbol |= 1 << BitLevelCount;
    while (Symbol != 1) {
        PriceIndex = LZP_GET_PRICE_INDEX(Probabilities[Symbol >> 1],
                                         Symbol & 0x1);

        Price += Prices[PriceIndex];
        Symbol >>= 1;
    }

    return Price;
}

ULONG
LzpRcTreeReverseGetPrice (
    PLZ_PROB Probabilities,
    INT BitLevelCount,
    ULONG Symbol,
    PULONG Prices
    )

/*++

Routine Description:

    This routine gets the reverse price for a given symbol.

Arguments:

    Probabilities - Supplies a pointer to the probabilities.

    BitLevelCount - Supplies the count of bit levels.

    Symbol - Supplies the symbol to get the price for.

    Prices - Supplies the array of prices.

Return Value:

    Returns the price for the given symbol.

--*/

{

    ULONG Bit;
    INT Level;
    ULONG Mask;
    ULONG Price;
    UINTN PriceIndex;

    Price = 0;
    Mask = 1;
    for (Level = BitLevelCount; Level != 0; Level -= 1) {
        Bit = Symbol & 0x1;
        Symbol >>= 1;
        PriceIndex = LZP_GET_PRICE_INDEX(Probabilities[Mask], Bit);
        Price += Prices[PriceIndex];
        Mask = (Mask << 1) | Bit;
    }

    return Price;
}

VOID
LzpLiteralEncoderEncode (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    ULONG Symbol
    )

/*++

Routine Description:

    This routine encodes a literal out to the range encoder.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder to initialize.

    Probabilities - Supplies a pointer to the range probabilities.

    Symbol - Supplies the literal to encode.

Return Value:

    None.

--*/

{

    //
    // Set the end bit, then shift out the 8 bits of the literal byte, most
    // significant first.
    //

    Symbol |= 0x100;
    do {
        LzpRangeEncodeBit(RangeEncoder,
                          Probabilities + (Symbol >> 8),
                          (Symbol >> 7) & 0x1);

        Symbol <<= 1;

    } while (Symbol < 0x10000);

    return;
}

VOID
LzpLiteralEncoderEncodeMatched (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    ULONG Symbol,
    ULONG MatchByte
    )

/*++

Routine Description:

    This routine encodes a matched symbol out to the range encoder.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder to initialize.

    Probabilities - Supplies a pointer to the range probabilities.

    Symbol - Supplies the literal to encode.

    MatchByte - Supplies the match byte.

Return Value:

    None.

--*/

{

    ULONG Offset;

    Offset = 0x100;
    Symbol |= 0x100;
    do {
        MatchByte <<= 1;
        LzpRangeEncodeBit(
               RangeEncoder,
               Probabilities + (Offset + (MatchByte & Offset) + (Symbol >> 8)),
               (Symbol >> 7) & 0x1);

        Symbol <<= 1;
        Offset &= ~(MatchByte ^ Symbol);

    } while (Symbol < 0x10000);

    return;
}

LZ_STATUS
LzpRangeEncoderInitialize (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes an LZMA range encoder.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder to initialize.

    Context - Supplies a pointer to the system context.

Return Value:

    LZ Status.

--*/

{

    RangeEncoder->System = NULL;
    if (RangeEncoder->DirectOutput == FALSE) {
        RangeEncoder->BufferBase =
                     Context->Reallocate(NULL, LZMA_RANGE_ENCODER_BUFFER_SIZE);

        if (RangeEncoder->BufferBase == NULL) {
            return LzErrorMemory;
        }

        RangeEncoder->BufferLimit = RangeEncoder->BufferBase +
                                    LZMA_RANGE_ENCODER_BUFFER_SIZE;
    }

    return LzSuccess;
}

VOID
LzpRangeEncoderDestroy (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys the given range encoder.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder to initialize.

    Context - Supplies a pointer to the system context.

Return Value:

    None.

--*/

{

    if ((RangeEncoder->BufferBase != NULL) &&
        (RangeEncoder->DirectOutput == FALSE)) {

        Context->Reallocate(RangeEncoder->BufferBase, 0);
    }

    RangeEncoder->BufferBase = NULL;
    return;
}

VOID
LzpRangeEncoderReset (
    PLZMA_RANGE_ENCODER RangeEncoder
    )

/*++

Routine Description:

    This routine resets an LZMA range encoder.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder.

Return Value:

    None.

--*/

{

    RangeEncoder->Low = 0;
    RangeEncoder->Range = 0xFFFFFFFF;
    RangeEncoder->CacheSize = 1;
    RangeEncoder->Cache = 0;
    RangeEncoder->Buffer = RangeEncoder->BufferBase;
    RangeEncoder->BufferRead = RangeEncoder->Buffer;
    RangeEncoder->Result = LzSuccess;
    return;
}

VOID
LzpRangeEncodeDirectBits (
    PLZMA_RANGE_ENCODER RangeEncoder,
    ULONG Value,
    ULONG BitCount
    )

/*++

Routine Description:

    This routine encodes several bits into the range encoder.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder.

    Value - Supplies the field of bits to encode, most significant bits first.

    BitCount - Supplies the number of valid bits in the value.

Return Value:

    None.

--*/

{

    do {
        RangeEncoder->Range >>= 1;
        BitCount -= 1;
        RangeEncoder->Low += RangeEncoder->Range &
                             (0UL - ((Value >> BitCount) & 0x1));

        if (RangeEncoder->Range < LZMA_RANGE_TOP_VALUE) {
            RangeEncoder->Range <<= 8;
            LzpRangeEncoderShiftLow(RangeEncoder);
        }

    } while (BitCount != 0);

    return;
}

VOID
LzpRangeEncodeBit (
    PLZMA_RANGE_ENCODER RangeEncoder,
    PLZ_PROB Probabilities,
    ULONG Symbol
    )

/*++

Routine Description:

    This routine encodes a single bit into the range encoder.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder.

    Probabilities - Supplies the distribution of symbol probabilities.

    Symbol - Supplies the symbol to encode.

Return Value:

    None.

--*/

{

    ULONG NewBound;
    ULONG Probability;

    Probability = *Probabilities;
    NewBound = (RangeEncoder->Range >> LZMA_BIT_MODEL_BIT_COUNT) * Probability;
    if (Symbol == 0) {
        RangeEncoder->Range = NewBound;
        Probability += (LZMA_BIT_MODEL_TOTAL - Probability) >>
                       LZMA_MOVE_BIT_COUNT;

    } else {
        RangeEncoder->Low += NewBound;
        RangeEncoder->Range -= NewBound;
        Probability -= Probability >> LZMA_MOVE_BIT_COUNT;
    }

    *Probabilities = Probability;
    if (RangeEncoder->Range < LZMA_RANGE_TOP_VALUE) {
        RangeEncoder->Range <<= 8;
        LzpRangeEncoderShiftLow(RangeEncoder);
    }

    return;
}

VOID
LzpRangeEncoderFlushData (
    PLZMA_RANGE_ENCODER RangeEncoder
    )

/*++

Routine Description:

    This routine flushes the remaining data out of the range encoder by pushing
    enough zero bits.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < LZMA_MOVE_BIT_COUNT; Index += 1) {
        LzpRangeEncoderShiftLow(RangeEncoder);
    }

    return;
}

VOID
LzpRangeEncoderShiftLow (
    PLZMA_RANGE_ENCODER RangeEncoder
    )

/*++

Routine Description:

    This routine shifts the range encoder into an appropriate range.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder.

Return Value:

    None.

--*/

{

    PUCHAR Buffer;
    UCHAR Cache;

    if (((ULONG)(RangeEncoder->Low) < 0xFF000000) ||
        ((UINTN)(RangeEncoder->Low >> 32) != 0)) {

        Cache = RangeEncoder->Cache;
        do {
            Buffer = RangeEncoder->Buffer;
            *Buffer = (UCHAR)(Cache + (UCHAR)(RangeEncoder->Low >> 32));
            Buffer += 1;
            RangeEncoder->Buffer = Buffer;
            if (Buffer == RangeEncoder->BufferLimit) {
                LzpRangeEncoderFlushStream(RangeEncoder);
            }

            Cache = 0xFF;
            RangeEncoder->CacheSize -= 1;

        } while (RangeEncoder->CacheSize != 0);

        RangeEncoder->Cache = (UCHAR)((ULONG)(RangeEncoder->Low) >> 24);
    }

    RangeEncoder->CacheSize += 1;
    RangeEncoder->Low = (ULONG)(RangeEncoder->Low) << 8;
    return;
}

VOID
LzpRangeEncoderFlushStream (
    PLZMA_RANGE_ENCODER RangeEncoder
    )

/*++

Routine Description:

    This routine writes the range encoder buffer out to the system.

Arguments:

    RangeEncoder - Supplies a pointer to the range encoder.

Return Value:

    None.

--*/

{

    INTN Size;
    PLZ_CONTEXT System;
    INTN Written;

    System = RangeEncoder->System;

    //
    // Don't bother if the encoder's already borked. If there is no write
    // function, then copy out ensures that there's always space.
    //

    if ((RangeEncoder->Result != LzSuccess) || (System->Write == NULL)) {

        //
        // This should really be an assert, as it should never happen.
        //

        if (RangeEncoder->Buffer >= RangeEncoder->BufferLimit) {
            RangeEncoder->Result = LzErrorOutputEof;
        }

        return;
    }

    Size = RangeEncoder->Buffer - RangeEncoder->BufferBase;
    Written = System->Write(System, RangeEncoder->BufferBase, Size);
    if (Size != Written) {
        RangeEncoder->Result = LzErrorWrite;
    }

    System->CompressedCrc32 = LzpComputeCrc32(System->CompressedCrc32,
                                              RangeEncoder->BufferBase,
                                              Size);

    System->CompressedSize += Size;
    RangeEncoder->Buffer = RangeEncoder->BufferBase;
    RangeEncoder->BufferRead = RangeEncoder->BufferBase;
    return;
}

BOOL
LzpLzmaCopyOutput (
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine copies encoder output to the user buffer.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    TRUE if all data from the output buffer was copied.

    FALSE if there is still more stuff in the output buffer.

--*/

{

    PLZMA_ENCODER Encoder;
    BOOL Finished;
    PLZMA_RANGE_ENCODER Range;
    UINTN Size;

    Encoder = Context->InternalState;
    Finished = TRUE;
    Range = &(Encoder->RangeEncoder);

    //
    // Copy as much as possible from the range encoder buffer.
    //

    if (Range->Buffer > Range->BufferRead) {
        Size = Range->Buffer - Range->BufferRead;
        if (Size > Context->OutputSize) {
            Size = Context->OutputSize;
            Finished = FALSE;
        }

        if (Range->DirectOutput == FALSE) {
            memcpy(Context->Output, Range->BufferRead, Size);
        }

        Context->CompressedCrc32 = LzpComputeCrc32(Context->CompressedCrc32,
                                                   Range->BufferRead,
                                                   Size);

        Context->CompressedSize += Size;
        Range->BufferRead += Size;
        Context->Output += Size;
        Context->OutputSize -= Size;

        //
        // Reset the range encoder buffer if it's all clear. Don't do that if
        // this is the user's buffer.
        //

        if ((Range->DirectOutput == FALSE) && (Finished != FALSE)) {
            Range->Buffer = Range->BufferBase;
            Range->BufferRead = Range->BufferBase;
        }
    }

    return Finished;
}


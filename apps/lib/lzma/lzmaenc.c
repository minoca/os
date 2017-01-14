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
    ((_RangeEncoder)->Processed + \
     ((_RangeEncoder)->Buffer - (_RangeEncoder)->BufferBase) + \
     (_RangeEncoder)->CacheSize)

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_BIG_HASH_DICT_LIMIT (1 << 24)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _LZMA_ENCODER_OUT_BUFFER {
    PUCHAR Data;
    UINTN Remaining;
    BOOL Overflow;
} LZMA_ENCODER_OUT_BUFFER, *PLZMA_ENCODER_OUT_BUFFER;

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
LzpLzmaMemoryEncode (
    PLZMA_ENCODER Encoder,
    PUCHAR Destination,
    PUINTN DestinationSize,
    PCUCHAR Source,
    UINTN SourceSize,
    BOOL WriteEndMark,
    PLZ_CONTEXT Context
    );

LZ_STATUS
LzpLzmaPrepareMemoryEncode (
    PLZMA_ENCODER Encoder,
    PCUCHAR Source,
    UINTN SourceSize,
    ULONG KeepWindowSize,
    PLZ_CONTEXT Context
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
    PLZMA_ENCODER Encoder
    );

VOID
LzpLzmaEncoderFinish (
    PLZMA_ENCODER Encoder
    );

LZ_STATUS
LzpLzmaEncodeOneBlock (
    PLZMA_ENCODER Encoder,
    BOOL UseLimits,
    ULONG MaxPackSize,
    ULONG MaxUnpackSize
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

INTN
LzpLzmaEncodeBufferWrite (
    PLZ_CONTEXT Context,
    PVOID Buffer,
    UINTN Size
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
LzLzmaEncoderInitializeProperties (
    PLZMA_ENCODER_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine initializes LZMA encoder properties to their defaults.

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
    Properties->WriteEndMark = FALSE;
    return;
}

LZ_STATUS
LzLzmaEncode (
    PUCHAR Destination,
    PUINTN DestinationSize,
    PCUCHAR Source,
    UINTN SourceSize,
    PLZMA_ENCODER_PROPERTIES Properties,
    PUCHAR EncodedProperties,
    PUINTN EncodedPropertiesSize,
    BOOL WriteEndMark,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine LZMA encodes the given data block.

Arguments:

    Destination - Supplies a pointer to buffer where the compressed data will
        be returned.

    DestinationSize - Supplies a pointer that on input contains the size of the
        destination buffer. On output, will contain the size of the encoded
        data.

    Source - Supplies a pointer to the data to compress.

    SourceSize - Supplies the number of bytes in the source buffer.

    Properties - Supplies a pointer to the encoding properties.

    EncodedProperties - Supplies a pointer where the encoded properties will be
        returned on success.

    EncodedPropertiesSize - Supplies a pointer that on input contains the size
        of the encoded properties buffer. On output, contains the size of the
        encoded properties.

    WriteEndMark - Supplies a boolean indicating whether or not to write an
        end marker.

    Context - Supplies a pointer to the general LZ context.

Return Value:

    LZ status.

--*/

{

    PLZMA_ENCODER Encoder;
    LZ_STATUS Result;

    Encoder = LzpLzmaCreateEncoder(Context);
    if (Encoder == NULL) {
        return LzErrorMemory;
    }

    Result = LzpLzmaEncoderSetProperties(Encoder, Properties);
    if (Result != LzSuccess) {
        goto LzmaEncodeEnd;
    }

    Result = LzpLzmaWriteProperties(Encoder,
                                    EncodedProperties,
                                    EncodedPropertiesSize);

    if (Result != LzSuccess) {
        goto LzmaEncodeEnd;
    }

    Result = LzpLzmaMemoryEncode(Encoder,
                                 Destination,
                                 DestinationSize,
                                 Source,
                                 SourceSize,
                                 WriteEndMark,
                                 Context);

LzmaEncodeEnd:
    LzpLzmaDestroyEncoder(Encoder, Context);
    return Result;
}

LZ_STATUS
LzLzmaEncodeStream (
    PLZMA_ENCODER_PROPERTIES Properties,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine LZMA encodes the given stream.

Arguments:

    Properties - Supplies the encoder properties to set.

    Context - Supplies a pointer to the general LZ context.

Return Value:

    LZ status.

--*/

{

    PLZMA_ENCODER Encoder;
    LZ_STATUS Result;

    Encoder = LzpLzmaCreateEncoder(Context);
    if (Encoder == NULL) {
        return LzErrorMemory;
    }

    Result = LzpLzmaEncoderSetProperties(Encoder, Properties);
    if (Result != LzSuccess) {
        goto LzmaEncodeStreamEnd;
    }

    //
    // Prepare the encoder.
    //

    Encoder->MatchFinderData.System = Context;
    Encoder->NeedInitialization = TRUE;
    Encoder->RangeEncoder.System = Context;
    Result = LzpLzmaAllocateBuffers(Encoder, 0, Context);
    if (Result != LzSuccess) {
        goto LzmaEncodeStreamEnd;
    }

    //
    // Run the encoder.
    //

    Result = LzpLzmaEncode(Encoder);

LzmaEncodeStreamEnd:
    LzpLzmaDestroyEncoder(Encoder, Context);
    return Result;
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
    LZ_STATUS Status;

    Encoder = Context->Reallocate(NULL, sizeof(LZMA_ENCODER));
    if (Encoder == NULL) {
        return NULL;
    }

    memset(Encoder, 0, sizeof(LZMA_ENCODER));
    Status = LzpRangeEncoderInitialize(&(Encoder->RangeEncoder), Context);
    if (Status != LzSuccess) {
        goto CreateEncoderEnd;
    }

    LzpInitializeMatchFinder(&(Encoder->MatchFinderData));
    LzLzmaEncoderInitializeProperties(&Properties);
    LzpLzmaEncoderSetProperties(Encoder, &Properties);
    LzpLzmaInitializeFastPosition(Encoder->FastPosition);
    LzpLzmaInitializePriceTables(Encoder->ProbabilityPrices);
    Encoder->LiteralProbabilities = NULL;
    Encoder->SaveState.LiteralProbabilities = NULL;
    Status = LzSuccess;

CreateEncoderEnd:
    if (Status != LzSuccess) {
        LzpLzmaDestroyEncoder(Encoder, Context);
    }

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
    Encoder->WriteEndMark = NewProperties.WriteEndMark;
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

    if (DictSize >= (1 >> 22)) {
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
LzpLzmaMemoryEncode (
    PLZMA_ENCODER Encoder,
    PUCHAR Destination,
    PUINTN DestinationSize,
    PCUCHAR Source,
    UINTN SourceSize,
    BOOL WriteEndMark,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine LZMA encodes the given data block in memory.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Destination - Supplies a pointer to buffer where the compressed data will
        be returned.

    DestinationSize - Supplies a pointer that on input contains the size of the
        destination buffer. On output, will contain the size of the encoded
        data.

    Source - Supplies a pointer to the data to compress.

    SourceSize - Supplies the number of bytes in the source buffer.

    WriteEndMark - Supplies a boolean indicating whether or not to write an
        end marker.

    Context - Supplies a pointer to the general LZ context.

Return Value:

    LZ status.

--*/

{

    LZ_CONTEXT LocalContext;
    LZMA_ENCODER_OUT_BUFFER Out;
    LZ_STATUS Result;

    Out.Data = Destination;
    Out.Remaining = *DestinationSize;
    Out.Overflow = FALSE;
    Encoder->WriteEndMark = WriteEndMark;
    memcpy(&LocalContext, Context, sizeof(LZ_CONTEXT));
    LocalContext.Write = LzpLzmaEncodeBufferWrite;
    LocalContext.WriteContext = &Out;
    Encoder->RangeEncoder.System = &LocalContext;
    Result = LzpLzmaPrepareMemoryEncode(Encoder,
                                        Source,
                                        SourceSize,
                                        FALSE,
                                        &LocalContext);

    if (Result != LzSuccess) {
        return Result;
    }

    Result = LzpLzmaEncode(Encoder);
    if ((Result == LzSuccess) && (Encoder->Position64 != SourceSize)) {
        Result = LzErrorFailure;
    }

    *DestinationSize -= Out.Remaining;
    if (Out.Overflow != FALSE) {
        return LzErrorOutputEof;
    }

    return Result;
}

LZ_STATUS
LzpLzmaPrepareMemoryEncode (
    PLZMA_ENCODER Encoder,
    PCUCHAR Source,
    UINTN SourceSize,
    ULONG KeepWindowSize,
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine prepares for a memory-based LZMA encoding.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Source - Supplies a pointer to the data to compress.

    SourceSize - Supplies the number of bytes in the source buffer.

    KeepWindowSize - Supplies the minimum window size to use.

    Context - Supplies a pointer to the general LZ context.

Return Value:

    LZ status.

--*/

{

    Encoder->MatchFinderData.DirectInput = TRUE;
    Encoder->MatchFinderData.BufferBase = (PUCHAR)Source;
    Encoder->MatchFinderData.DirectInputRemaining = SourceSize;
    Encoder->NeedInitialization = TRUE;
    return LzpLzmaAllocateBuffers(Encoder, KeepWindowSize, Context);
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

    Result = LzpMatchFinderAllocateBuffers(&(Encoder->MatchFinderData),
                                           Encoder->DictSize,
                                           BeforeSize,
                                           Encoder->FastByteCount,
                                           LZMA_MAX_MATCH_LENGTH,
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
    Encoder->Position64 = 0;
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

    Encoder->OptimumEndIndex = 0;
    Encoder->OptimumCurrentIndex = 0;
    Encoder->AdditionalOffset = 0;
    Encoder->PbMask = (1 << Encoder->Pb) - 1;
    Encoder->LpMask = (1 << Encoder->Lc) - 1;
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
    PLZMA_ENCODER Encoder
    )

/*++

Routine Description:

    This routine implements the block encoding loop on a fully initialized and
    reset LZMA encoder.

Arguments:

    Encoder - Supplies a pointer to the encoder.

Return Value:

    LZ Status.

--*/

{

    PLZMA_RANGE_ENCODER RangeEncoder;
    PLZ_REPORT_PROGRESS ReportProgress;
    LZ_STATUS Result;

    RangeEncoder = &(Encoder->RangeEncoder);
    ReportProgress = RangeEncoder->System->ReportProgress;
    while (TRUE) {
        Result = LzpLzmaEncodeOneBlock(Encoder, FALSE, 0, 0);
        if ((Result != LzSuccess) || (Encoder->Finished != FALSE)) {
            break;
        }

        if (ReportProgress != NULL) {
            Result = ReportProgress(RangeEncoder->System,
                                    Encoder->Position64,
                                    LzpRangeEncoderGetProcessed(RangeEncoder));

            if (Result != LzSuccess) {
                Result = LzErrorProgress;
                break;
            }
        }
    }

    LzpLzmaEncoderFinish(Encoder);
    return Result;
}

VOID
LzpLzmaEncoderFinish (
    PLZMA_ENCODER Encoder
    )

/*++

Routine Description:

    This routine performs completion activities associated with a finished
    encoder.

Arguments:

    Encoder - Supplies a pointer to the encoder.

Return Value:

    None.

--*/

{

    //
    // If there were multiple threads, this would be where the multithreaded
    // match finder would be cleaned up.
    //

    return;
}

LZ_STATUS
LzpLzmaEncodeOneBlock (
    PLZMA_ENCODER Encoder,
    BOOL UseLimits,
    ULONG MaxPackSize,
    ULONG MaxUnpackSize
    )

/*++

Routine Description:

    This routine implements the crux of the LZMA encoder, the encoding of a
    single block.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    UseLimits - Supplies a boolean indicating whether the next two parameters
        are valid and should be honored.

    MaxPackSize - Supplies the maximum encoded block size.

    MaxUnpackSize - Supplies the maximum decoded block size.

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

    Range = &(Encoder->RangeEncoder);
    CurrentPosition32 = (ULONG)(Encoder->Position64);
    StartPosition32 = CurrentPosition32;

    //
    // Handle the very beginning of the stream. Watch out for an empty
    // stream, otherwise emit the first literal since there's nothing to
    // repeat.
    //

    if (Encoder->Position64 == 0) {
        if (Encoder->MatchFinder.GetCount(Encoder->MatchFinderContext) == 0) {
            return LzpLzmaEncoderFlush(Encoder, CurrentPosition32);
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
        goto LzmaEncodeOneBlockEnd;
    }

    while (TRUE) {

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

            } else if (Processed >= (1 << 17)) {
                Encoder->Position64 += CurrentPosition32 - StartPosition32;
                return LzpLzmaEncoderGetError(Encoder);
            }
        }
    }

LzmaEncodeOneBlockEnd:
    Encoder->Position64 += CurrentPosition32 - StartPosition32;
    return LzpLzmaEncoderFlush(Encoder, CurrentPosition32);
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

INTN
LzpLzmaEncodeBufferWrite (
    PLZ_CONTEXT Context,
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine performs a simple write operation to a buffer for the memory
    encode version of the encoder.

Arguments:

    Context - Supplies a pointer to the LZ context.

    Buffer - Supplies a pointer where the read data should be returned for
        read operations, or where the data to write exists for write oprations.

    Size - Supplies the number of bytes to read or write.

Return Value:

    Returns the number of bytes read or written. For writes, anything other
    than the full write size is considered failure. Reads however can return
    less than asked for. If a read returns zero, that indicates end of file.

    -1 on I/O failure.

--*/

{

    PLZMA_ENCODER_OUT_BUFFER Out;

    Out = Context->WriteContext;
    if (Out->Remaining < Size) {
        Size = Out->Remaining;
        Out->Overflow = TRUE;
    }

    memcpy(Out->Data, Buffer, Size);
    Out->Remaining -= Size;
    Out->Data += Size;
    return Size;
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
        Symbol -= LZMA_LENGTH_LOW_SYMBOLS;
        if (Symbol < LZMA_LENGTH_LOW_SYMBOLS + LZMA_LENGTH_MID_SYMBOLS) {
            LzpRangeEncodeBit(RangeEncoder, &(LengthEncoder->Choice2), 0);
            LzpRcTreeEncode(
                  RangeEncoder,
                  LengthEncoder->Mid + (PositionState << LZMA_LENGTH_MID_BITS),
                  LZMA_LENGTH_MID_BITS,
                  Symbol);

        //
        // The value is greater than the mid cutoff, so 11 is written, then the
        // value (minus the mid cutoff).
        //

        } else {
            Symbol -= LZMA_LENGTH_MID_SYMBOLS;
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
    RangeEncoder->BufferBase =
                     Context->Reallocate(NULL, LZMA_RANGE_ENCODER_BUFFER_SIZE);

    if (RangeEncoder->BufferBase == NULL) {
        return LzErrorMemory;
    }

    RangeEncoder->BufferLimit = RangeEncoder->BufferBase +
                                LZMA_RANGE_ENCODER_BUFFER_SIZE;

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

    Context->Reallocate(RangeEncoder->BufferBase, 0);
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
    RangeEncoder->Processed = 0;
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
    INTN Written;

    if (RangeEncoder->Result != LzSuccess) {
        return;
    }

    Size = RangeEncoder->Buffer - RangeEncoder->BufferBase;
    Written = RangeEncoder->System->Write(RangeEncoder->System,
                                          RangeEncoder->BufferBase,
                                          Size);

    if (Size != Written) {
        RangeEncoder->Result = LzErrorWrite;
    }

    RangeEncoder->Processed += Size;
    RangeEncoder->Buffer = RangeEncoder->BufferBase;
    return;
}


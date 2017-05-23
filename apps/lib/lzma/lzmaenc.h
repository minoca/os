/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzmaenc.h

Abstract:

    This header contains internal definitions for the LZMA encoder.

Author:

    Evan Green 13-Dec-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "lzfind.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Get the index into the price array for a given probability and a given
// symbol.
//

#define LZP_GET_PRICE_INDEX(_Probability, _Symbol) \
    (((_Probability) ^ ((-((INT)(_Symbol))) & (LZMA_BIT_MODEL_TOTAL - 1))) >> \
     LZMA_MOVE_REDUCING_BITS)

//
// This macro gets the price for a given probability and symbol.
//

#define LZP_GET_PRICE(_Encoder, _Probability, _Symbol) \
    (_Encoder)->ProbabilityPrices[LZP_GET_PRICE_INDEX(_Probability, _Symbol)]

//
// This macro returns the literal probability pointer for the given previous
// byte.
//

#define LZP_LITERAL_PROBABILITIES(_Encoder, _Position, _PreviousByte) \
    ((_Encoder)->LiteralProbabilities + \
     ((((_Position) & (_Encoder)->LpMask) << (_Encoder)->Lc) + \
      ((_PreviousByte) >> (8 - (_Encoder)->Lc))) * (ULONG)0x300)

#define LZP_IS_CHARACTER_STATE(_State) ((_State) < 7)

#define LZP_GET_LENGTH_TO_POSITION_STATE(_Length) \
    (((_Length) < LZMA_LENGTH_TO_POSITION_STATES + 1) ? \
     ((_Length) - 2) : \
     (LZMA_LENGTH_TO_POSITION_STATES - 1))

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_OPTIMAL_COUNT (1 << 12)

#define LZMA_PB_MAX 4
#define LZMA_LC_MAX 8
#define LZMA_LP_MAX 4

#define LZMA_MAX_PB_STATES (1 << LZMA_PB_MAX)
#define LZMA_MOVE_REDUCING_BITS 4
#define LZMA_BIT_PRICE_SHIFT_BITS 4

#define LZMA_INITIAL_PROBABILITY (LZMA_BIT_MODEL_TOTAL >> 1)

#define LZMA_MIN_DICT_LOG 6
#define LZMA_MAX_DICT_LOG 32
#define LZMA_DISTANCE_TABLE_MAX (LZMA_MAX_DICT_LOG * 2)

#define LZMA_DICT_LOG_BITS (9 + (sizeof(UINTN) / 2))
#define LZMA_DICT_LOG_MAX_COMPRESS (((LZMA_DICT_LOG_BITS - 1) * 2) + 7)

#define LZMA_RANGE_ENCODER_BUFFER_SIZE (1 << 16)

#define LZMA_MAX_MATCH_LENGTH \
    (LZMA_MIN_MATCH_LENGTH + LZMA_LENGTH_TOTAL_SYMBOL_COUNT - 1)

#define LZMA_ALIGN_MASK (LZMA_ALIGN_TABLE_SIZE - 1)

#define LZMA_INFINITY_PRICE (1 << 30)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef ULONG LZMA_STATE;

/*++

Structure Description:

    This structure stores the pricing information for an encoding subproblem.

Members:

    Price - Stores the price of the option, that is the number of post range-
        encoding bits needed to encode the string.

    State - Stores the encoder state value after packets in this solution. This
        is computed only after the best subproblem solution has been found.

    PreviousIsCharacter - Stores whether or not the tail packets to this
        subproblem contain more than one packet (in which case the second to
        last is a literal, because the only options are 1) LIT LONGREP[0] and
        2) MATCH LIT LONGREP[0].

    Previous2 - Stores a boolean indicating whether the "tail" contains 3
        packets. This is only valid if the previous is character boolean is set.

    PositionPrevious2 - Stores the uncompressed size of the substring encoded
        by all packets except the "tail". This is only valid if Previous2 and
        PreviousIsCharacter are TRUE.

    BackPrevious2 - Stores -1 if the first packet in the tail is a LIT, 0-3 if
        it is a REP, or 4 + distance if it is a MATCH. This is only valid if
        Previous2 and PreviousIsCharacter are TRUE.

    PositionPrevious - Stores the uncompressed size of the substring encoded by
        all packets except the last one.

    BackPrevious - Stores -1 if the last packet is a LIT, 0-3 if it is a REP,
        or 4 + distance if it is a MATCH. This is always 0 if
        PreviousIsCharacter is TRUE, since the last packet can only be a
        LONGREP[0] in that case).

    Backs - Stores the values of the 4 last used distances after the packets
        in this solution. This is computed only after the best subproblem
        solution has been determined.

--*/

typedef struct _LZMA_OPTIMAL {
    ULONG Price;
    LZMA_STATE State;
    BOOL PreviousIsCharacter;
    BOOL Previous2;
    ULONG PositionPrevious2;
    ULONG BackPrevious2;
    ULONG PositionPrevious;
    ULONG BackPrevious;
    ULONG Backs[LZMA_REP_COUNT];
} LZMA_OPTIMAL, *PLZMA_OPTIMAL;

/*++

Structure Description:

    This structure stores the state for the range encoder.

Members:

    Range - Stores the total range being encoded.

    Cache - Stores the cached not yet encoded bits.

    Low - Stores the range low value, a 33-bit value.

    CacheSize - Stores the number of 0xFF bytes which need to be shifted out.
        It needs to be large enough to store the size of the uncompressed input.

    Buffer - Stores the current output buffer position to write to.

    BufferLimit - Stores one beyond the last valid byte to write to.

    BufferBase - Stores the initial buffer address.

    BufferRead - Stores the next pointer to read output from if partial copies
        have occurred.

    System - Stores the system context.

    Result - Stores the resulting status code.

    DirectOutput - Stores a boolean indicating whether or not the buffer was
        allocated (FALSE) or not (TRUE).

--*/

typedef struct _LZMA_RANGE_ENCODER {
    ULONG Range;
    UCHAR Cache;
    ULONGLONG Low;
    ULONGLONG CacheSize;
    PUCHAR Buffer;
    PUCHAR BufferLimit;
    PUCHAR BufferBase;
    PUCHAR BufferRead;
    PLZ_CONTEXT System;
    LZ_STATUS Result;
    BOOL DirectOutput;
} LZMA_RANGE_ENCODER, *PLZMA_RANGE_ENCODER;

/*++

Structure Description:

    This structure stores the distance encoder probability information.

Members:

    Choice - Stores the zero bit probability.

    Choice2 - Stores the one bit probability.

    Low - Stores the low symbols probabilities, used for match lengths after
        a bit sequence of 0.

    Mid - Stores the middle symbols probabilities, used for match lengths after
        a bit sequence of 10.

    High - Stores the high symbols probabilities, used for match lengths after
        a bit sequence of 11.

--*/

typedef struct _LZMA_LENGTH_ENCODER {
    LZ_PROB Choice;
    LZ_PROB Choice2;
    LZ_PROB Low[LZMA_MAX_PB_STATES << LZMA_LENGTH_LOW_BITS];
    LZ_PROB Mid[LZMA_MAX_PB_STATES << LZMA_LENGTH_MID_BITS];
    LZ_PROB High[LZMA_LENGTH_HIGH_SYMBOLS];
} LZMA_LENGTH_ENCODER, *PLZMA_LENGTH_ENCODER;

/*++

Structure Description:

    This structure stores the distance pricing state.

Members:

    LengthEncoder - Stores the length encoder itself.

    TableSize - Stores the number of symbols in the table.

    Prices - Stores the pricing information for each state and each symbol.

    Counters - Stores the counters which count down to when the price tables
        are updated.

--*/

typedef struct _LZMA_LENGTH_PRICE_ENCODER {
    LZMA_LENGTH_ENCODER LengthEncoder;
    ULONG TableSize;
    ULONG Prices[LZMA_MAX_PB_STATES][LZMA_LENGTH_TOTAL_SYMBOL_COUNT];
    ULONG Counters[LZMA_MAX_PB_STATES];
} LZMA_LENGTH_PRICE_ENCODER, *PLZMA_LENGTH_PRICE_ENCODER;

/*++

Structure Description:

    This structure stores the saved encoder state.

Members:

    LiteralProbabilities - Stores a pointer to the saved literal probabilities.

    Reps - Stores the last for reps.

    IsMatch - Stores the saved match probabilities.

    IsRep - Stores the saved rep probabilities.

    IsRepG0 - Stores the saved rep G0 probabilities.

    IsRepG1 - Stores the saved rep G1 probabilities.

    IsRepG2 - Stores the saved rep G2 probabilities.

    IsRep0Long - Stores the saved rep 0 long probabilities.

--*/

typedef struct _LZMA_SAVE_STATE {
    PLZ_PROB LiteralProbabilities;
    ULONG Reps[LZMA_REP_COUNT];
    LZ_PROB IsMatch[LZMA_STATE_COUNT][LZMA_MAX_PB_STATES];
    LZ_PROB IsRep[LZMA_STATE_COUNT];
    LZ_PROB IsRepG0[LZMA_STATE_COUNT];
    LZ_PROB IsRepG1[LZMA_STATE_COUNT];
    LZ_PROB IsRepG2[LZMA_STATE_COUNT];
    LZ_PROB IsRep0Long[LZMA_STATE_COUNT][LZMA_MAX_PB_STATES];
} LZMA_SAVE_STATE, *PLZMA_SAVE_STATE;

/*++

Structure Description:

    This structure stores the LZMA encoder state.

Members:

    MatchFinderContext - Stores the context for the match finder.

    MatchFinder - Stores the function interface to the match finder.

    OptimumEndIndex - Stores the last index operated on by the optimzer.

    OptimumCurrentIndex - Stores the current index being operated on by the
        optimizer.

    LongestMatchLength - Stores the length of the longest match seen.

    PairCount - Stores the number of previous matches available now.

    AvailableCount - Stores the number of bytes available from the match finder.

    FastByteCount - Stores the number of bytes above which any match is deemed
        acceptable.

    AdditionalOffset - Stores extra offset from what the match finder reports.

    Reps - Stores the last few reps, which can be reused by the encoder.

    State - Stores the current encoder state.

    Lc - Stores the lc parameter, which is the number of high bits of the
        previous byte to use as context for literal encoding.

    Lp - Stores the lp parameter, which is the number of low bits of the
        dictionary position to include in the literal position state.

    Pb - Stores the pb parameter, which is the number of low bits of the
        dictionary position to include in the position state.

    LpMask - Stores the lp parameter as a mask.

    PbMask - Stores the pb parameter as a mask.

    LcLp - Stores Lc + Lp.

    Stage - Stores the stage of encoding the encoder is currently in.

    LiteralProbabilities - Stores the probabilities for each literal.

    FastMode - Stores a boolean indicating whether the encoder is in fast mode.

    WriteEndMark - Stores a boolean indicating whether the encoder should
        write an end mark or not.

    Finished - Stores a boolean indicating whether or not the encoder is
        finished.

    Multithread - Stores a boolean indicating whether the encoder is
        multithreaded or not.

    NeedInitialization - Stores a boolean indicating whether or not the
        encoder still needs initialization.

    FileWrapper - Stores a boolean indicating whether the file header and
        footer check fields should be written to the stream.

    MatchPriceCount - Stores the count of match prices.

    AlignPriceCount - Stores the count of align prices.

    DistanceTableSize - Stores the size of the distance table.

    DictSize - Stores the size of the symbol dictionary.

    Result - Stores the resulting status code.

    RangeEncoder - Stores the range encoder state.

    MatchFinderData - Stores the match finder state.

    Processed - Stores the number of input bytes that have been completely
        processed so far.

    Optimal - Stores the match prices.

    FastPosition - Stores a quicker log lookup table for calcuating the
        slot for a given position.

    ProbabilityPrices - Stores the prices for a given symbol and given
        probability. Symbol really means group of symbols since only the least
        few low bits are used.

    Matches - Stores the array of match (pairs of distance and length) found
        by the match finder.

    SlotPrices - Stores the array of prices for each slot

    DistancesPrices - Stores the array of distances prices.

    AlignPrices - Stores the array of align prices.

    IsMatch - Stores the probability group used at the start of a packet for
        each state and position state.

    IsRep - Stores the probability group used after a bit sequence of 1 for
        each state.

    IsRepG0 - Stores the probability group used after a bit sequence of 11 for
        each state.

    IsRepG1 - Stores the probability group used after a bit sequence of 111 for
        each state.

    IsRepG2 - Stores the probability group used after a bit sequence of 1111
        for each state.

    IsRep0Long - Stores the probability group used after a bit sequence of 110
        for each state and position state.

    SlotEncoder - Stores the position slot encoder probabilities.

    Encoders - Stores the position encoders.

    AlignEncoder - Stores the align encoder, used for distance for 14+
        distance slots.

    LengthEncoder - Stores the length encoder state.

    RepLengthEncoder - Stores the rep length encoder state.

    SaveState - Stores the saved state.

--*/

typedef struct _LZMA_ENCODER {
    PVOID MatchFinderContext;
    LZ_MATCH_FINDER_INTERFACE MatchFinder;
    ULONG OptimumEndIndex;
    ULONG OptimumCurrentIndex;
    ULONG LongestMatchLength;
    ULONG PairCount;
    ULONG AvailableCount;
    ULONG FastByteCount;
    ULONG AdditionalOffset;
    ULONG Reps[LZMA_REP_COUNT];
    ULONG State;
    ULONG Lc;
    ULONG Lp;
    ULONG Pb;
    ULONG LpMask;
    ULONG PbMask;
    ULONG LcLp;
    LZMA_STAGE Stage;
    PLZ_PROB LiteralProbabilities;
    BOOL FastMode;
    BOOL WriteEndMark;
    BOOL Finished;
    BOOL Multithread;
    BOOL NeedInitialization;
    BOOL FileWrapper;
    ULONG MatchPriceCount;
    ULONG AlignPriceCount;
    ULONG DistanceTableSize;
    ULONG DictSize;
    LZ_STATUS Result;
    LZMA_RANGE_ENCODER RangeEncoder;
    LZ_MATCH_FINDER MatchFinderData;
    ULONGLONG Processed;
    LZMA_OPTIMAL Optimal[LZMA_OPTIMAL_COUNT];
    UCHAR FastPosition[1 << LZMA_DICT_LOG_BITS];
    ULONG ProbabilityPrices[LZMA_BIT_MODEL_TOTAL >> LZMA_MOVE_REDUCING_BITS];
    ULONG Matches[(LZMA_MAX_MATCH_LENGTH * 2) + 3];
    ULONG SlotPrices[LZMA_LENGTH_TO_POSITION_STATES][LZMA_DISTANCE_TABLE_MAX];
    ULONG DistancesPrices[LZMA_LENGTH_TO_POSITION_STATES][LZMA_FULL_DISTANCES];
    ULONG AlignPrices[LZMA_ALIGN_TABLE_SIZE];
    LZ_PROB IsMatch[LZMA_STATE_COUNT][LZMA_MAX_PB_STATES];
    LZ_PROB IsRep[LZMA_STATE_COUNT];
    LZ_PROB IsRepG0[LZMA_STATE_COUNT];
    LZ_PROB IsRepG1[LZMA_STATE_COUNT];
    LZ_PROB IsRepG2[LZMA_STATE_COUNT];
    LZ_PROB IsRep0Long[LZMA_STATE_COUNT][LZMA_MAX_PB_STATES];
    LZ_PROB SlotEncoder[LZMA_LENGTH_TO_POSITION_STATES][LZMA_POSITION_SLOTS];
    LZ_PROB Encoders[LZMA_FULL_DISTANCES - LZMA_END_POSITION_MODEL_INDEX];
    LZ_PROB AlignEncoder[LZMA_POSITION_SLOTS];
    LZMA_LENGTH_PRICE_ENCODER LengthEncoder;
    LZMA_LENGTH_PRICE_ENCODER RepLengthEncoder;
    LZMA_SAVE_STATE SaveState;
} LZMA_ENCODER, *PLZMA_ENCODER;

//
// -------------------------------------------------------------------- Globals
//

extern const UCHAR LzLzmaLiteralNextStates[LZMA_STATE_COUNT];
extern const UCHAR LzLzmaMatchNextStates[LZMA_STATE_COUNT];
extern const UCHAR LzLzmaRepNextStates[LZMA_STATE_COUNT];
extern const UCHAR LzLzmaShortRepNextStates[LZMA_STATE_COUNT];

//
// -------------------------------------------------------- Function Prototypes
//

//
// Internal encoder functions
//

ULONG
LzpLzmaReadMatchDistances (
    PLZMA_ENCODER Encoder,
    PULONG DistancePairCount
    );

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

ULONG
LzpLzmaGetPositionSlot (
    PLZMA_ENCODER Encoder,
    ULONG Position
    );

/*++

Routine Description:

    This routine returns the slot associated with the given position.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Position - Supplies the current position.

Return Value:

    Returns the slot associated with the given position.

--*/

ULONG
LzpLzmaGetPositionSlot2 (
    PLZMA_ENCODER Encoder,
    ULONG Position
    );

/*++

Routine Description:

    This routine returns the slot associated with the given position.

Arguments:

    Encoder - Supplies a pointer to the encoder.

    Position - Supplies the current position.

Return Value:

    Returns the slot associated with the given position.

--*/

//
// Optimizer functions
//

ULONG
LzpLzmaGetOptimumFast (
    PLZMA_ENCODER Encoder,
    ULONG Position,
    PULONG BackResult
    );

/*++

Routine Description:

    This routine determines the optimum repeat code of previous data to emit.

Arguments:

    Encoder - Supplies a pointer to the LZMA encoder.

    Position - Supplies the current input position.

    BackResult - Supplies a pointer where either a distance or the index into
        the previously used distances array will be returned for repeats. If
        this is less than LZMA_REP_COUNT, then it is an index into a previously
        used distance array. If it is greater than or equal to LZMA_REP_COUNT,
        then LZMA_REP_COUNT should be subtracted and it is a distance back
        into the data.

Return Value:

    Returns the length of the best match.

--*/

ULONG
LzpLzmaGetOptimum (
    PLZMA_ENCODER Encoder,
    ULONG Position,
    PULONG BackResult
    );

/*++

Routine Description:

    This routine determines the optimum repeat code of previous data to emit.

Arguments:

    Encoder - Supplies a pointer to the LZMA encoder.

    Position - Supplies the current input position.

    BackResult - Supplies a pointer where either a distance or the index into
        the previously used distances array will be returned for repeats. If
        this is less than LZMA_REP_COUNT, then it is an index into a previously
        used distance array. If it is greater than or equal to LZMA_REP_COUNT,
        then LZMA_REP_COUNT should be subtracted and it is a distance back
        into the data.

Return Value:

    Returns the length of the best match.

--*/


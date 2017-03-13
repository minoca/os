/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzmadec.c

Abstract:

    This module implements an LZMA decoder, based on Igor Pavlov's 7z decoder.

Author:

    Evan Green 10-Mar-2017

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

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the number of probabilities in the array.
//

#define LZMA_PROBABILITIES_COUNT(_Decoder)  \
    (LzmaProbLiteral +                      \
     ((ULONG)LZMA_LITERAL_SIZE << ((_Decoder)->Lc + (_Decoder)->Lp)))

//
// Define primitives used in the primary decoding function. These are defined
// as macros rather than routines for performance.
//

#define LZMA_NORMALIZE_RANGE(_Range, _Code, _Buffer)    \
    if ((_Range) < LZMA_RANGE_TOP_VALUE) {              \
        (_Range) <<= 8;                                 \
        (_Code) = ((_Code) << 8) | *(_Buffer);          \
        (_Buffer) += 1;                                 \
    }

#define LZMA_RANGE_READ(_Prob, _ProbValue, _Bound, _Range, _Code, _Buffer) \
    (_ProbValue) = *(_Prob);                                               \
    LZMA_NORMALIZE_RANGE(_Range, _Code, _Buffer);                          \
    (_Bound) = ((_Range) >> LZMA_BIT_MODEL_BIT_COUNT) * (_ProbValue);

#define LZMA_RANGE_IS_BIT_0(_Code, _Bound) ((_Code) < (_Bound))

#define LZMA_RANGE_UPDATE_0(_Prob, _ProbValue, _Range, _Bound)      \
    (_Range) = (_Bound);                                            \
    *(_Prob) = (LZ_PROB)((_ProbValue) +                             \
                         ((LZMA_BIT_MODEL_TOTAL - (_ProbValue)) >>  \
                          LZMA_MOVE_BIT_COUNT));

#define LZMA_RANGE_UPDATE_1(_Prob, _ProbValue, _Range, _Bound, _Code)   \
    (_Range) -= (_Bound);                                               \
    (_Code) -= (_Bound);                                                \
    *(_Prob) = (LZ_PROB)((_ProbValue) - ((_ProbValue) >> LZMA_MOVE_BIT_COUNT));

#define LZMA_RANGE_GET_BIT(_Prob, _ProbValue, _Bound, _Range, _Code, _BitOut) \
    if (LZMA_RANGE_IS_BIT_0(_Code, _Bound)) {                                 \
        LZMA_RANGE_UPDATE_0(_Prob, _ProbValue, _Range, _Bound);               \
        (_BitOut) <<= 1;                                                      \
                                                                              \
    } else {                                                                  \
        LZMA_RANGE_UPDATE_1(_Prob, _ProbValue, _Range, _Bound, _Code);        \
        (_BitOut) = ((_BitOut) << 1) | 0x1;                                   \
    }

//
// Define all the same macros but that are only attempting to do it. This means
// that 1) they don't update the probabilities and 2) they error out if they
// need to go beyond the given input buffer.
//

#define LZMA_NORMALIZE_RANGE_ATTEMPT(_Range, _Code, _Buffer, _BufferEnd)    \
    if ((_Range) < LZMA_RANGE_TOP_VALUE) {                                  \
        if ((_Buffer) >= (_BufferEnd)) {                                    \
            return LzmaAttemptError;                                        \
        }                                                                   \
                                                                            \
        (_Range) <<= 8;                                                     \
        (_Code) = ((_Code) << 8) | *(_Buffer);                              \
        (_Buffer) += 1;                                                     \
    }

#define LZMA_RANGE_READ_ATTEMPT(_Prob,                                     \
                                _ProbValue,                                \
                                _Bound,                                    \
                                _Range,                                    \
                                _Code,                                     \
                                _Buffer,                                   \
                                _BufferEnd)                                \
                                                                           \
    (_ProbValue) = *(_Prob);                                               \
    LZMA_NORMALIZE_RANGE_ATTEMPT(_Range, _Code, _Buffer, _BufferEnd);      \
    (_Bound) = ((_Range) >> LZMA_BIT_MODEL_BIT_COUNT) * (_ProbValue);

#define LZMA_RANGE_UPDATE_0_ATTEMPT(_Range, _Bound) \
    (_Range) = (_Bound);                            \

#define LZMA_RANGE_UPDATE_1_ATTEMPT(_Range, _Bound, _Code)  \
    (_Range) -= (_Bound);                                   \
    (_Code) -= (_Bound);                                    \

#define LZMA_RANGE_GET_BIT_ATTEMPT(_Bound, _Range, _Code, _BitOut) \
    if (LZMA_RANGE_IS_BIT_0(_Code, _Bound)) {                      \
        LZMA_RANGE_UPDATE_0_ATTEMPT(_Range, _Bound);               \
        (_BitOut) <<= 1;                                           \
                                                                   \
    } else {                                                       \
        LZMA_RANGE_UPDATE_1_ATTEMPT( _Range, _Bound, _Code);       \
        (_BitOut) = ((_BitOut) << 1) | 0x1;                        \
    }

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default stream working buffer size and the minimum working buffer
// size.
//

#define LZMA_DECODE_DEFAULT_WORKING_SIZE 0x100000
#define LZMA_DECODE_MIN_WORKING_SIZE 512

//
// Define the minimum input stream size.
//

#define LZMA_MINIMUM_SIZE 5

#define LZMA_LITERAL_SIZE 0x300

//
// Define the maximum size of an LZMA input symbol. The maximum number of bits
// is log2((2^11 / 31) ^ 22) + 26 = 134 + 26 = 160.
//

#define LZMA_MAX_INPUT 20

//
// Define the maximum number of bits that contribute to position encoding.
//

#define LZMA_MAX_POSITION_BITS 4
#define LZMA_MAX_POSITION_STATES (1 << LZMA_MAX_POSITION_BITS)

#define LZMA_MATCH_SPEC_LENGTH_START \
    (LZMA_MIN_MATCH_LENGTH + LZMA_LENGTH_TOTAL_SYMBOL_COUNT)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define offsets into the length encoder regions region of the probabilities
// array.
//

typedef enum _LZMA_LENGTH_PROBABILITIES_OFFSET {
    LzmaLengthChoice = 0,
    LzmaLengthChoice2,
    LzmaLengthLow,
    LzmaLengthMid = LzmaLengthLow +
                    (LZMA_MAX_POSITION_STATES << LZMA_LENGTH_LOW_BITS),

    LzmaLengthHigh = LzmaLengthMid +
                     (LZMA_MAX_POSITION_STATES << LZMA_LENGTH_LOW_BITS),

    LzmaLengthCount = LzmaLengthHigh + LZMA_LENGTH_HIGH_SYMBOLS
} LZMA_LENGTH_PROBABILITIES_OFFSET, *PLZMA_LENGTH_PROBABILITIES_OFFSET;

//
// Define offsets into the probabilities array.
//

typedef enum _LZMA_PROBABILITIES_OFFSET {
    LzmaProbIsMatch = 0,
    LzmaProbIsRep = LzmaProbIsMatch +
                    (LZMA_STATE_COUNT << LZMA_MAX_POSITION_BITS),

    LzmaProbIsRepG0 = LzmaProbIsRep + LZMA_STATE_COUNT,
    LzmaProbIsRepG1 = LzmaProbIsRepG0 + LZMA_STATE_COUNT,
    LzmaProbIsRepG2 = LzmaProbIsRepG1 + LZMA_STATE_COUNT,
    LzmaProbIsRep0Long = LzmaProbIsRepG2 + LZMA_STATE_COUNT,
    LzmaProbPositionSlot = LzmaProbIsRep0Long +
                           (LZMA_STATE_COUNT << LZMA_MAX_POSITION_BITS),

    LzmaProbSpecPosition = LzmaProbPositionSlot +
                           (LZMA_LENGTH_TO_POSITION_STATES <<
                            LZMA_POSITION_SLOT_BITS),

    LzmaProbAlign = LzmaProbSpecPosition +
                    (LZMA_FULL_DISTANCES - LZMA_END_POSITION_MODEL_INDEX),

    LzmaProbLengthCoder = LzmaProbAlign + LZMA_ALIGN_TABLE_SIZE,
    LzmaProbRepLengthEncoder = LzmaProbLengthCoder + LzmaLengthCount,
    LzmaProbLiteral = LzmaProbRepLengthEncoder + LzmaLengthCount,
} LZMA_PROBABILITIES_OFFSET, *PLZMA_PROBABILITIES_OFFSET;

typedef enum _LZMA_DECODE_ATTEMPT {
    LzmaAttemptError,
    LzmaAttemptLiteral,
    LzmaAttemptMatch,
    LzmaAttemptRep
} LZMA_DECODE_ATTEMPT, *PLZMA_DECODE_ATTEMPT;

/*++

Structure Description:

    This structure stores the LZMA decoder context.

Members:

    Lc - Stores the lc parameter, which is the number of high bits of the
        previous byte to use as context for literal encoding.

    Lp - Stores the lp parameter, which is the number of low bits of the
        dictionary position to include in the literal position state.

    Pb - Stores the pb parameter, which is the number of low bits of the
        dictionary position to include in the position state.

    DictSize - Stores the dictionary size as read from the stream properties.

    Probabilities - Stores a pointer to the range decoder probabilities.

    Dict - Stores a pointer to the decoding dictionary, which is a circular
        buffer.

    Buffer - Stores the source buffer to decode from.

    Range - Stores the range decoder's current range.

    Code - Stores the range decoder's current value.

    DictPosition - Stores the current size of the dictionary.

    DictBufferSize - Stores the size of the dictionary buffer allocation.

    ProcessedPosition - Stores the output stream position.

    CheckDictSize - Stores the currently decoded dictionary size.

    State - Stores the current decoder state.

    Reps - Stores the last four repeat values, used by opcodes to repeat them.

    RemainingLength - Stores the number of bytes remaining in the compressed
        input stream.

    NeedFlush - Stores a boolean indicating if the range decoder needs to be
        flushed.

    NeedReset - Stores a boolean indicating if the probabilities and state
        need to be reset.

    ProbabilityCount - Stores the length of the probabilities array.

    WorkingSize - Stores the number of valid bytes in the working buffer.

    Working - Stores the buffer used to decode the current symbol.

--*/

typedef struct _LZMA_DECODER {
    ULONG Lc;
    ULONG Lp;
    ULONG Pb;
    ULONG DictSize;
    PLZ_PROB Probabilities;
    PUCHAR Dict;
    PCUCHAR Buffer;
    ULONG Range;
    ULONG Code;
    UINTN DictPosition;
    UINTN DictBufferSize;
    ULONG ProcessedPosition;
    ULONG CheckDictSize;
    ULONG State;
    ULONG Reps[LZMA_REP_COUNT];
    ULONG RemainingLength;
    BOOL NeedFlush;
    BOOL NeedReset;
    ULONG ProbabilityCount;
    ULONG WorkingSize;
    UCHAR Working[LZMA_MAX_INPUT];
} LZMA_DECODER, *PLZMA_DECODER;

//
// ----------------------------------------------- Internal Function Prototypes
//

LZ_STATUS
LzpLzmaDecoderInitialize (
    PLZ_CONTEXT Context,
    PCUCHAR Properties,
    ULONG PropertiesSize,
    PLZMA_DECODER Decoder
    );

VOID
LzpLzmaDecoderDestroy (
    PLZ_CONTEXT Context,
    PLZMA_DECODER Decoder
    );

LZ_STATUS
LzpLzmaDecodeProperties (
    PCUCHAR Properties,
    ULONG PropertiesSize,
    PLZMA_DECODER Decoder
    );

VOID
LzpLzmaDecoderReset (
    PLZMA_DECODER Decoder
    );

VOID
LzpLzmaDecoderInitializeState (
    PLZMA_DECODER Decoder,
    BOOL InitializeDictionary,
    BOOL InitializeState
    );

LZ_STATUS
LzpLzmaDecodeToBuffer (
    PLZMA_DECODER Decoder,
    PUCHAR Destination,
    PUINTN DestinationSize,
    PCUCHAR Source,
    PUINTN SourceSize,
    BOOL HasEndMark,
    PLZ_COMPLETION_STATUS CompletionStatus
    );

LZ_STATUS
LzpLzmaDecodeToDictionary (
    PLZMA_DECODER Decoder,
    UINTN DictLimit,
    PCUCHAR Source,
    PUINTN SourceSize,
    BOOL HasEndMark,
    PLZ_COMPLETION_STATUS CompletionStatus
    );

VOID
LzpLzmaDecoderResetState (
    PLZMA_DECODER Decoder
    );

LZ_STATUS
LzpLzmaDecodeLoop (
    PLZMA_DECODER Decoder,
    UINTN Limit,
    PCUCHAR BufferEnd
    );

LZ_STATUS
LzpLzmaDecode (
    PLZMA_DECODER Decoder,
    UINTN Limit,
    PCUCHAR BufferEnd
    );

LZMA_DECODE_ATTEMPT
LzpLzmaAttemptDecode (
    PLZMA_DECODER Decoder,
    PCUCHAR Buffer,
    UINTN InSize
    );

VOID
LzpLzmaDecoderWriteRemainder (
    PLZMA_DECODER Decoder,
    UINTN Limit
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LZ_STATUS
LzLzmaDecode (
    PLZ_CONTEXT Context,
    PUCHAR Destination,
    PUINTN DestinationSize,
    PCUCHAR Source,
    PUINTN SourceSize,
    PCUCHAR Properties,
    ULONG PropertiesSize,
    BOOL HasEndMark,
    PLZ_COMPLETION_STATUS CompletionStatus
    )

/*++

Routine Description:

    This routine decompresses a block of LZMA encoded data in a single shot.
    It is an error if the destination buffer is not big enough to hold the
    decompressed data.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

    Destination - Supplies a pointer where the uncompressed data should be
        written.

    DestinationSize - Supplies a pointer that on input contains the size of the
        uncompressed data buffer. On output this will be updated to contain
        the number of valid bytes in the destination buffer.

    Source - Supplies a pointer to the compressed data.

    SourceSize - Supplies a pointer that on input contains the size of the
        source buffer. On output this will be updated to contain the number of
        source bytes consumed.

    Properties - Supplies a pointer to the properties bytes.

    PropertiesSize - Supplies the number of properties bytes in byte given
        buffer.

    HasEndMark - Supplies a boolean indicating whether an end mark is expected
        within this block of data. Supply TRUE if the compressed stream was
        finished with an end mark. Supply FALSE if the given data ends when the
        source buffer ends.

    CompletionStatus - Supplies a pointer where the completion status will be
        returned indicating whether an end mark was found or more data is
        expected. This field only has meaning if the decoding chews through
        the entire source buffer.

Return Value:

    Returns an LZ status code indicating overall success or failure.

--*/

{

    LZMA_DECODER Decoder;
    UINTN InSize;
    UINTN OriginalBufferSize;
    PVOID OriginalDict;
    UINTN OutSize;
    LZ_STATUS Status;

    OriginalBufferSize = 0;
    OriginalDict = NULL;
    OutSize = *DestinationSize;
    InSize = *SourceSize;
    *DestinationSize = 0;
    *SourceSize = 0;
    *CompletionStatus = LzCompletionNotSpecified;
    if (InSize < LZMA_MINIMUM_SIZE) {
        return LzErrorInputEof;
    }

    Status = LzpLzmaDecoderInitialize(Context,
                                      Properties,
                                      PropertiesSize,
                                      &Decoder);

    if (Status != LzSuccess) {
        return Status;
    }

    //
    // Put the destination buffer in as the dictionary to avoid the copy.
    //

    OriginalDict = Decoder.Dict;
    OriginalBufferSize = Decoder.DictBufferSize;
    Decoder.Dict = Destination;
    Decoder.DictBufferSize = OutSize;
    *SourceSize = InSize;
    Status = LzpLzmaDecodeToDictionary(&Decoder,
                                       OutSize,
                                       Source,
                                       SourceSize,
                                       HasEndMark,
                                       CompletionStatus);

    *DestinationSize = Decoder.DictPosition;
    if ((Status == LzSuccess) &&
        (*CompletionStatus == LzCompletionMoreInputRequired)) {

        Status = LzErrorInputEof;
    }

    //
    // Restore the original dict so the right thing gets freed.
    //

    Decoder.Dict = OriginalDict;
    Decoder.DictBufferSize = OriginalBufferSize;
    LzpLzmaDecoderDestroy(Context, &Decoder);
    return Status;
}

LZ_STATUS
LzLzmaDecodeStream (
    PLZ_CONTEXT Context,
    PVOID InBuffer,
    UINTN InBufferSize,
    PVOID OutBuffer,
    UINTN OutBufferSize
    )

/*++

Routine Description:

    This routine decompresses a stream of LZMA encoded data.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

    InBuffer - Supplies an optional pointer to the working buffer to use for
        input. If not supplied, a working buffer will be allocated.

    InBufferSize - Supplies the size of the optional working input buffer.
        Supply zero if none was provided.

    OutBuffer - Supplies an optional pointer to the working buffer to use for
        output. If not supplied, a working buffer will be allocated.

    OutBufferSize - Supplies the size of the optional working input buffer.
        Supply zero if none was provided.

Return Value:

    Returns an LZ status code indicating overall success or failure.

--*/

{

    PVOID AllocatedIn;
    PVOID AllocatedOut;
    INTN BytesComplete;
    LZ_COMPLETION_STATUS CompletionStatus;
    LZMA_DECODER Decoder;
    BOOL DecoderInitialized;
    UINTN InPosition;
    UINTN InProcessed;
    UINTN InSize;
    UINTN OutPosition;
    UINTN OutProcessed;
    LZ_STATUS Status;

    DecoderInitialized = FALSE;
    AllocatedIn = NULL;
    AllocatedOut = NULL;
    InSize = 0;
    OutPosition = 0;

    //
    // Allocate a working buffer if none was provided.
    //

    if (InBuffer == NULL) {
        InBufferSize = LZMA_DECODE_DEFAULT_WORKING_SIZE;
        AllocatedIn = Context->Reallocate(NULL, InBufferSize);
        if (AllocatedIn == NULL) {
            return LzErrorMemory;
        }

        InBuffer = AllocatedIn;

    } else if (InBufferSize < LZMA_DECODE_MIN_WORKING_SIZE) {
        return LzErrorUnsupported;
    }

    if (OutBuffer == NULL) {
        OutBufferSize = LZMA_DECODE_DEFAULT_WORKING_SIZE;
        AllocatedOut = Context->Reallocate(NULL, OutBufferSize);
        if (AllocatedOut == NULL) {
            return LzErrorMemory;
        }

        OutBuffer = AllocatedOut;

    } else if (OutBufferSize < LZMA_DECODE_MIN_WORKING_SIZE) {
        return LzErrorUnsupported;
    }

    //
    // Attempt to read the property bytes out of the stream.
    //

    do {
        BytesComplete = Context->Read(Context,
                                      InBuffer + InSize,
                                      InBufferSize - InSize);

        if (BytesComplete == 0) {
            Status = LzErrorInputEof;
            goto LzmaDecodeStreamEnd;

        } else if (BytesComplete < 0) {
            Status = LzErrorRead;
            goto LzmaDecodeStreamEnd;
        }

        InSize += BytesComplete;

    } while (InSize < LZMA_PROPERTIES_SIZE);

    Status = LzpLzmaDecoderInitialize(Context,
                                      InBuffer,
                                      LZMA_PROPERTIES_SIZE,
                                      &Decoder);

    if (Status != LzSuccess) {
        goto LzmaDecodeStreamEnd;
    }

    DecoderInitialized = TRUE;
    InPosition = LZMA_PROPERTIES_SIZE;

    //
    // Loop decoding the stream.
    //

    while (TRUE) {
        if (InPosition >= InSize) {
            InSize = Context->Read(Context, InBuffer, InBufferSize);
            if (InSize < 0) {
                Status = LzErrorRead;
                goto LzmaDecodeStreamEnd;
            }

            InPosition = 0;
        }

        InProcessed = InSize - InPosition;
        OutProcessed = OutBufferSize - OutPosition;
        Status = LzpLzmaDecodeToBuffer(&Decoder,
                                       OutBuffer + OutPosition,
                                       &OutProcessed,
                                       InBuffer + InPosition,
                                       &InProcessed,
                                       FALSE,
                                       &CompletionStatus);

        InPosition += InProcessed;
        OutPosition += OutProcessed;
        BytesComplete = Context->Write(Context, OutBuffer, OutPosition);
        if (BytesComplete != OutPosition) {
            Status = LzErrorWrite;
            goto LzmaDecodeStreamEnd;
        }

        OutPosition = 0;
        if (Status != LzSuccess) {
            goto LzmaDecodeStreamEnd;
        }

        if ((InProcessed == 0) || (OutProcessed == 0)) {
            if (CompletionStatus != LzCompletionFinishedWithMark) {
                Status = LzErrorCorruptData;
            }

            break;
        }
    }

LzmaDecodeStreamEnd:
    if (AllocatedIn != NULL) {
        Context->Reallocate(AllocatedIn, 0);
    }

    if (AllocatedOut != NULL) {
        Context->Reallocate(AllocatedOut, 0);
    }

    if (DecoderInitialized != FALSE) {
        LzpLzmaDecoderDestroy(Context, &Decoder);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

LZ_STATUS
LzpLzmaDecoderInitialize (
    PLZ_CONTEXT Context,
    PCUCHAR Properties,
    ULONG PropertiesSize,
    PLZMA_DECODER Decoder
    )

/*++

Routine Description:

    This routine initializes the LZMA decoder.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

    Properties - Supplies a pointer to the properties bytes.

    PropertiesSize - Supplies the number of properties bytes in byte given
        buffer.

    Decoder - Supplies a pointer to the decoder to initialize.

Return Value:

    LZ Status code.

--*/

{

    UINTN DictBufferSize;
    ULONG DictSize;
    ULONG Mask;
    ULONG ProbabilitiesCount;
    LZ_STATUS Status;

    Decoder->Dict = NULL;
    Decoder->Probabilities = NULL;
    Decoder->ProbabilityCount = 0;
    Status = LzpLzmaDecodeProperties(Properties, PropertiesSize, Decoder);
    if (Status != LzSuccess) {
        return Status;
    }

    ProbabilitiesCount = LZMA_PROBABILITIES_COUNT(Decoder);
    Decoder->Probabilities =
               Context->Reallocate(NULL, ProbabilitiesCount * sizeof(LZ_PROB));

    if (Decoder->Probabilities == NULL) {
        return LzErrorMemory;
    }

    Decoder->ProbabilityCount = ProbabilitiesCount;

    //
    // Allocate the dictionary.
    //

    DictSize = Decoder->DictSize;
    Mask = (1 << 12) - 1;
    if (DictSize >= (1 << 30)) {
        Mask = (1 << 22) - 1;

    } else if (DictSize >= (1UL << 22)) {
        Mask = (1 << 20) - 1;
    }

    DictBufferSize = ((UINTN)DictSize + Mask) & ~Mask;
    if (DictBufferSize < DictSize) {
        DictBufferSize = DictSize;
    }

    Decoder->Dict = Context->Reallocate(NULL, DictBufferSize);
    if (Decoder->Dict == NULL) {
        Status = LzErrorMemory;
        goto LzmaDecoderInitializeEnd;
    }

    Decoder->DictBufferSize = DictBufferSize;
    LzpLzmaDecoderReset(Decoder);
    Status = LzSuccess;

LzmaDecoderInitializeEnd:
    if (Status != LzSuccess) {
        if (Decoder->Probabilities != NULL) {
            Context->Reallocate(Decoder->Probabilities, 0);
            Decoder->Probabilities = NULL;
        }
    }

    return LzSuccess;
}

VOID
LzpLzmaDecoderDestroy (
    PLZ_CONTEXT Context,
    PLZMA_DECODER Decoder
    )

/*++

Routine Description:

    This routine destroys the inner contents of an LZMA decoder.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

    Decoder - Supplies a pointer to the decoder to tear down.

Return Value:

    LZ Status code.

--*/

{

    if (Decoder->Probabilities != NULL) {
        Context->Reallocate(Decoder->Probabilities, 0);
        Decoder->Probabilities = NULL;
    }

    if (Decoder->Dict != NULL) {
        Context->Reallocate(Decoder->Dict, 0);
        Decoder->Dict = NULL;
    }

    return;
}

LZ_STATUS
LzpLzmaDecodeProperties (
    PCUCHAR Properties,
    ULONG PropertiesSize,
    PLZMA_DECODER Decoder
    )

/*++

Routine Description:

    This routine decodes the LZMA properties bytes and sets them into the
    decoder.

Arguments:

    Properties - Supplies a pointer to the properties bytes.

    PropertiesSize - Supplies the number of properties bytes in byte given
        buffer.

    Decoder - Supplies a pointer to the decoder to initialize.

Return Value:

    LZ Status code.

--*/

{

    UCHAR Parameters;

    if (PropertiesSize < LZMA_PROPERTIES_SIZE) {
        return LzErrorUnsupported;
    }

    Decoder->DictSize = Properties[1] |
                        ((ULONG)(Properties[2]) << 8) |
                        ((ULONG)(Properties[3]) << 16) |
                        ((ULONG)(Properties[4]) << 24);

    if (Decoder->DictSize < LZMA_MINIMUM_DICT_SIZE) {
        Decoder->DictSize = LZMA_MINIMUM_DICT_SIZE;
    }

    Parameters = Properties[0];
    if (Parameters >= (9 * 5 * 5)) {
        return LzErrorUnsupported;
    }

    Decoder->Lc = Parameters % 9;
    Parameters /= 9;
    Decoder->Pb = Parameters / 5;
    Decoder->Lp = Parameters % 5;
    return LzSuccess;
}

VOID
LzpLzmaDecoderReset (
    PLZMA_DECODER Decoder
    )

/*++

Routine Description:

    This routine requests a reset of the decoder.

Arguments:

    Decoder - Supplies a pointer to the decoder.

Return Value:

    None.

--*/

{

    Decoder->DictPosition = 0;
    LzpLzmaDecoderInitializeState(Decoder, TRUE, TRUE);
    return;
}

VOID
LzpLzmaDecoderInitializeState (
    PLZMA_DECODER Decoder,
    BOOL InitializeDictionary,
    BOOL InitializeState
    )

/*++

Routine Description:

    This routine requests a reinitialization of the decoder dictionary and/or
    state.

Arguments:

    Decoder - Supplies a pointer to the decoder.

    InitializeDictionary - Supplies a boolean indicating whether or not to
        initialize the dictionary.

    InitializeState - Supplies a boolean indicating whether or not to
        initialize the state.

Return Value:

    None.

--*/

{

    Decoder->NeedFlush = TRUE;
    Decoder->RemainingLength = 0;
    Decoder->WorkingSize = 0;
    if (InitializeDictionary != FALSE) {
        Decoder->ProcessedPosition = 0;
        Decoder->CheckDictSize = 0;
        Decoder->NeedReset = 1;

    } else if (InitializeState != FALSE) {
        Decoder->NeedReset = 1;
    }

    return;
}

LZ_STATUS
LzpLzmaDecodeToBuffer (
    PLZMA_DECODER Decoder,
    PUCHAR Destination,
    PUINTN DestinationSize,
    PCUCHAR Source,
    PUINTN SourceSize,
    BOOL HasEndMark,
    PLZ_COMPLETION_STATUS CompletionStatus
    )

/*++

Routine Description:

    This routine decodes from an already initialized decoder to a memory
    buffer.

Arguments:

    Decoder - Supplies a pointer to the decoder.

    Destination - Supplies a pointer where the uncompressed data should be
        written.

    DestinationSize - Supplies a pointer that on input contains the size of the
        uncompressed data buffer. On output this will be updated to contain
        the number of valid bytes in the destination buffer.

    Source - Supplies a pointer to the compressed data.

    SourceSize - Supplies a pointer that on input contains the size of the
        source buffer. On output this will be updated to contain the number of
        source bytes consumed.

    HasEndMark - Supplies a boolean indicating whether an end mark is expected
        within this block of data. Supply TRUE if the compressed stream was
        finished with an end mark. Supply FALSE if the given data ends when the
        source buffer ends.

    CompletionStatus - Supplies a pointer where the completion status will be
        returned indicating whether an end mark was found or more data is
        expected. This field only has meaning if the decoding chews through
        the entire source buffer.

Return Value:

    Returns an LZ status code indicating overall success or failure.

--*/

{

    BOOL CurrentHasEndMark;
    UINTN CurrentInSize;
    UINTN CurrentOutSize;
    UINTN DictPosition;
    UINTN InSize;
    UINTN OutSize;
    LZ_STATUS Status;

    InSize = *SourceSize;
    OutSize = *DestinationSize;
    *DestinationSize = 0;
    *SourceSize = 0;
    while (TRUE) {
        CurrentInSize = InSize;
        if (Decoder->DictPosition == Decoder->DictBufferSize) {
            Decoder->DictPosition = 0;
        }

        DictPosition = Decoder->DictPosition;
        if (OutSize > Decoder->DictBufferSize - DictPosition) {
            CurrentOutSize = Decoder->DictBufferSize;
            CurrentHasEndMark = FALSE;

        } else {
            CurrentOutSize = DictPosition + OutSize;
            CurrentHasEndMark = HasEndMark;
        }

        Status = LzpLzmaDecodeToDictionary(Decoder,
                                           CurrentOutSize,
                                           Source,
                                           &CurrentInSize,
                                           CurrentHasEndMark,
                                           CompletionStatus);

        Source += CurrentInSize;
        InSize -= CurrentInSize;
        *SourceSize += CurrentInSize;
        CurrentOutSize = Decoder->DictPosition - DictPosition;
        memcpy(Destination, Decoder->Dict + DictPosition, CurrentOutSize);
        Destination += CurrentOutSize;
        OutSize -= CurrentOutSize;
        *DestinationSize += CurrentOutSize;
        if ((Status != LzSuccess) || (CurrentOutSize == 0) || (OutSize == 0)) {
            break;
        }
    }

    return Status;
}

LZ_STATUS
LzpLzmaDecodeToDictionary (
    PLZMA_DECODER Decoder,
    UINTN DictLimit,
    PCUCHAR Source,
    PUINTN SourceSize,
    BOOL HasEndMark,
    PLZ_COMPLETION_STATUS CompletionStatus
    )

/*++

Routine Description:

    This routine decompresses a block of LZMA encoded data.

Arguments:

    Decoder - Supplies a pointer to the decoder.

    DictLimit - Supplies the size limit of uncompressed data.

    Source - Supplies a pointer to the compressed data.

    SourceSize - Supplies a pointer that on input contains the size of the
        source buffer. On output this will be updated to contain the number of
        source bytes consumed.

    HasEndMark - Supplies a boolean indicating whether an end mark is expected
        within this block of data. Supply TRUE if the compressed stream was
        finished with an end mark. Supply FALSE if the given data ends when the
        source buffer ends.

    CompletionStatus - Supplies a pointer where the completion status will be
        returned indicating whether an end mark was found or more data is
        expected. This field only has meaning if the decoding chews through
        the entire source buffer.

Return Value:

    LZ Status code.

--*/

{

    LZMA_DECODE_ATTEMPT Attempt;
    BOOL CheckEndMark;
    PCUCHAR End;
    UINTN InSize;
    ULONG Lookahead;
    UINTN Processed;
    ULONG Remaining;
    LZ_STATUS Status;

    InSize = *SourceSize;
    *SourceSize = 0;
    LzpLzmaDecoderWriteRemainder(Decoder, DictLimit);
    *CompletionStatus = LzCompletionNotSpecified;
    while (Decoder->RemainingLength != LZMA_MATCH_SPEC_LENGTH_START) {

        //
        // Flush the range coder if needed.
        //

        if (Decoder->NeedFlush != FALSE) {
            while ((InSize > 0) && (Decoder->WorkingSize < LZMA_MINIMUM_SIZE)) {
                Decoder->Working[Decoder->WorkingSize] = *Source;
                Decoder->WorkingSize += 1;
                Source += 1;
                *SourceSize += 1;
                InSize -= 1;
            }

            if (Decoder->WorkingSize < LZMA_MINIMUM_SIZE) {
                *CompletionStatus = LzCompletionMoreInputRequired;
                return LzSuccess;
            }

            if (Decoder->Working[0] != 0) {
                return LzErrorCorruptData;
            }

            Decoder->Code = ((ULONG)Decoder->Working[1] << 24) |
                            ((ULONG)Decoder->Working[2] << 16) |
                            ((ULONG)Decoder->Working[3] << 8) |
                            (ULONG)Decoder->Working[4];

            Decoder->Range = 0xFFFFFFFF;
            Decoder->NeedFlush = FALSE;
            Decoder->WorkingSize = 0;
        }

        CheckEndMark = FALSE;
        if (Decoder->DictPosition >= DictLimit) {
            if ((Decoder->RemainingLength == 0) && (Decoder->Code == 0)) {
                *CompletionStatus = LzCompletionMaybeFinishedWithoutMark;
                return LzSuccess;
            }

            if (HasEndMark == FALSE) {
                *CompletionStatus = LzCompletionNotFinished;
                return LzSuccess;
            }

            if (Decoder->RemainingLength != 0) {
                *CompletionStatus = LzCompletionNotFinished;
                return LzErrorCorruptData;
            }

            CheckEndMark = TRUE;
        }

        if (Decoder->NeedReset != FALSE) {
            LzpLzmaDecoderResetState(Decoder);
        }

        if (Decoder->WorkingSize == 0) {
            if ((InSize < LZMA_MAX_INPUT) || (CheckEndMark != FALSE)) {
                Attempt = LzpLzmaAttemptDecode(Decoder, Source, InSize);
                if (Attempt == LzmaAttemptError) {
                    memcpy(Decoder->Working, Source, InSize);
                    Decoder->WorkingSize = InSize;
                    *SourceSize += InSize;
                    *CompletionStatus = LzCompletionMoreInputRequired;
                    return LzSuccess;
                }

                if ((CheckEndMark != FALSE) && (Attempt != LzmaAttemptMatch)) {
                    *CompletionStatus = LzCompletionNotFinished;
                    return LzErrorCorruptData;
                }

                End = Source;

            } else {
                End = Source + InSize - LZMA_MAX_INPUT;
            }

            Decoder->Buffer = Source;
            Status = LzpLzmaDecodeLoop(Decoder, DictLimit, End);
            if (Status != LzSuccess) {
                return Status;
            }

            Processed = Decoder->Buffer - Source;
            *SourceSize += Processed;
            Source += Processed;
            InSize -= Processed;

        } else {
            Remaining = Decoder->WorkingSize;
            Lookahead = 0;
            while ((Remaining < LZMA_MAX_INPUT) && (Lookahead < InSize)) {
                Decoder->Working[Remaining] = Source[Lookahead];
                Remaining += 1;
                Lookahead += 1;
            }

            Decoder->WorkingSize = Remaining;
            if ((Remaining < LZMA_MAX_INPUT) || (CheckEndMark != FALSE)) {
                Attempt = LzpLzmaAttemptDecode(Decoder,
                                               Decoder->Working,
                                               Remaining);

                if (Attempt == LzmaAttemptError) {
                    *SourceSize += Lookahead;
                    *CompletionStatus = LzCompletionMoreInputRequired;
                    return LzSuccess;
                }

                if ((CheckEndMark != FALSE) && (Attempt != LzmaAttemptMatch)) {
                    *CompletionStatus = LzCompletionNotFinished;
                    return LzErrorCorruptData;
                }
            }

            Decoder->Buffer = Decoder->Working;
            Status = LzpLzmaDecodeLoop(Decoder, DictLimit, Decoder->Buffer);
            if (Status != LzSuccess) {
                return Status;
            }

            Processed = Decoder->Buffer - Decoder->Working;
            if (Remaining < Processed) {
                return LzErrorCorruptData;
            }

            Remaining -= Processed;
            if (Lookahead < Remaining) {
                return LzErrorCorruptData;
            }

            Lookahead -= Remaining;
            *SourceSize += Lookahead;
            Source += Lookahead;
            InSize -= Lookahead;
            Decoder->WorkingSize = 0;
        }
    }

    if (Decoder->Code == 0) {
        *CompletionStatus = LzCompletionFinishedWithMark;
        return LzSuccess;
    }

    return LzErrorCorruptData;
}

VOID
LzpLzmaDecoderResetState (
    PLZMA_DECODER Decoder
    )

/*++

Routine Description:

    This routine resets the internal state of the LZMA decoder.

Arguments:

    Decoder - Supplies a pointer to the decoder.

Return Value:

    None.

--*/

{

    UINTN Index;
    PLZ_PROB Probabilities;
    UINTN ProbabilityCount;

    ProbabilityCount = Decoder->ProbabilityCount;
    Probabilities = Decoder->Probabilities;

    //
    // Reset all the probabilities to 50/50.
    //

    for (Index = 0; Index < ProbabilityCount; Index += 1) {
        Probabilities[Index] = LZMA_BIT_MODEL_TOTAL >> 1;
    }

    for (Index = 0; Index < LZMA_REP_COUNT; Index += 1) {
        Decoder->Reps[Index] = 1;
    }

    Decoder->State = 0;
    Decoder->NeedReset = FALSE;
    return;
}

LZ_STATUS
LzpLzmaDecodeLoop (
    PLZMA_DECODER Decoder,
    UINTN Limit,
    PCUCHAR BufferEnd
    )

/*++

Routine Description:

    This routine calls the decode function in a loop.

Arguments:

    Decoder - Supplies a pointer to the decoder.

    Limit - Supplies the dictionary size limit.

    BufferEnd - Supplies a pointer one beyond the end of the destination buffer.

Return Value:

    LZ Status.

--*/

{

    UINTN CurrentLimit;
    ULONG Remainder;
    LZ_STATUS Status;

    do {
        CurrentLimit = Limit;
        if (Decoder->CheckDictSize == 0) {
            Remainder = Decoder->DictSize - Decoder->ProcessedPosition;
            if (Limit - Decoder->DictPosition > Remainder) {
                CurrentLimit = Decoder->DictPosition + Remainder;
            }
        }

        Status = LzpLzmaDecode(Decoder, CurrentLimit, BufferEnd);
        if (Status != LzSuccess) {
            return Status;
        }

        if ((Decoder->CheckDictSize == 0) &&
            (Decoder->ProcessedPosition >= Decoder->DictSize)) {

            Decoder->CheckDictSize = Decoder->DictSize;
        }

        LzpLzmaDecoderWriteRemainder(Decoder, Limit);

    } while ((Decoder->DictPosition < Limit) &&
             (Decoder->Buffer < BufferEnd) &&
             (Decoder->RemainingLength < LZMA_MATCH_SPEC_LENGTH_START));

    if (Decoder->RemainingLength > LZMA_MATCH_SPEC_LENGTH_START) {
        Decoder->RemainingLength = LZMA_MATCH_SPEC_LENGTH_START;
    }

    return LzSuccess;
}

LZ_STATUS
LzpLzmaDecode (
    PLZMA_DECODER Decoder,
    UINTN Limit,
    PCUCHAR BufferEnd
    )

/*++

Routine Description:

    This routine decodes symbols from the input stream.

Arguments:

    Decoder - Supplies a pointer to the decoder.

    Limit - Supplies the dictionary size limit.

    BufferEnd - Supplies a pointer one beyond the end of the destination buffer.

Return Value:

    LZ Status.

--*/

{

    ULONG Bit;
    ULONG Bound;
    PCUCHAR Buffer;
    ULONG CheckDictSize;
    ULONG Code;
    PUCHAR CopyEnd;
    ULONG CurrentLength;
    PUCHAR Destination;
    PUCHAR Dict;
    UINTN DictBufferSize;
    UINTN DictIndex;
    UINTN DictPosition;
    ULONG DirectBits;
    ULONG Distance;
    ULONG Lc;
    ULONG Length;
    ULONG LengthLimit;
    PLZ_PROB LengthProb;
    PLZ_PROB LiteralProb;
    ULONG LpMask;
    ULONG Mask;
    ULONG MatchByte;
    ULONG Offset;
    ULONG PbMask;
    UINTN Position;
    ULONG PositionSlot;
    ULONG PositionState;
    PLZ_PROB Prob;
    PLZ_PROB Probs;
    LZ_PROB ProbValue;
    ULONG ProcessedPosition;
    ULONG Range;
    ULONG Remainder;
    ULONG Rep0;
    ULONG Rep1;
    ULONG Rep2;
    ULONG Rep3;
    UINTN Source;
    ULONG State;
    ULONG Symbol;

    Buffer = Decoder->Buffer;
    CheckDictSize = Decoder->CheckDictSize;
    Code = Decoder->Code;
    Dict = Decoder->Dict;
    DictBufferSize = Decoder->DictBufferSize;
    DictPosition = Decoder->DictPosition;
    Lc = Decoder->Lc;
    Length = 0;
    LpMask = (1 << Decoder->Lp) - 1;
    PbMask = (1 << Decoder->Pb) - 1;
    ProcessedPosition = Decoder->ProcessedPosition;
    Probs = Decoder->Probabilities;
    Range = Decoder->Range;
    Rep0 = Decoder->Reps[0];
    Rep1 = Decoder->Reps[1];
    Rep2 = Decoder->Reps[2];
    Rep3 = Decoder->Reps[3];
    State = Decoder->State;
    do {
        PositionState = ProcessedPosition & PbMask;
        Prob = Probs + LzmaProbIsMatch + (State << LZMA_MAX_POSITION_BITS) +
               PositionState;

        //
        // If the first bit is a zero, then it's a literal byte coming next.
        //

        LZMA_RANGE_READ(Prob, ProbValue, Bound, Range, Code, Buffer);
        if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
            LZMA_RANGE_UPDATE_0(Prob, ProbValue, Range, Bound);
            Prob = Probs + LzmaProbLiteral;
            if ((ProcessedPosition != 0) || (CheckDictSize != 0)) {
                DictIndex = DictPosition;
                if (DictPosition == 0) {
                    DictIndex = DictBufferSize;
                }

                Prob += (ULONG)LZMA_LITERAL_SIZE *
                        (((ProcessedPosition & LpMask) << Lc) +
                         (Dict[DictIndex - 1] >> (8 - Lc)));
            }

            ProcessedPosition += 1;

            //
            // If in a literal state, decode a literal byte.
            //

            if (State < LZMA_LITERAL_STATE_COUNT) {
                if (State < 4) {
                    State = 0;

                } else {
                    State -= 3;
                }

                Symbol = 0x1;
                do {
                    LZMA_RANGE_READ(Prob + Symbol,
                                    ProbValue,
                                    Bound,
                                    Range,
                                    Code,
                                    Buffer);

                    LZMA_RANGE_GET_BIT(Prob + Symbol,
                                       ProbValue,
                                       Bound,
                                       Range,
                                       Code,
                                       Symbol);

                } while (Symbol < 0x100);

            //
            // Decode a matched literal byte.
            //

            } else {
                DictIndex = DictPosition - Rep0;
                if (DictPosition < Rep0) {
                    DictIndex += DictBufferSize;
                }

                MatchByte = Dict[DictIndex];
                Offset = 0x100;
                if (State < 10) {
                    State -= 3;

                } else {
                    State -= 6;
                }

                Symbol = 0x1;
                do {
                    MatchByte <<= 1;
                    Bit = MatchByte & Offset;
                    LiteralProb = Prob + Offset + Bit + Symbol;
                    LZMA_RANGE_READ(LiteralProb,
                                    ProbValue,
                                    Bound,
                                    Range,
                                    Code,
                                    Buffer);

                    if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                        Offset &= ~Bit;

                    } else {
                        Offset &= Bit;
                    }

                    LZMA_RANGE_GET_BIT(LiteralProb,
                                       ProbValue,
                                       Bound,
                                       Range,
                                       Code,
                                       Symbol);

                } while (Symbol < 0x100);
            }

            //
            // Spit out the literal (truncating that extra high bit off there).
            //

            Dict[DictPosition] = (UCHAR)Symbol;
            DictPosition += 1;
            continue;
        }

        //
        // The first bit is a one, it's a repeat of some kind.
        //

        LZMA_RANGE_UPDATE_1(Prob, ProbValue, Range, Bound, Code);
        Prob = Probs + LzmaProbIsRep + State;
        LZMA_RANGE_READ(Prob, ProbValue, Bound, Range, Code, Buffer);

        //
        // 1 + 0 is a MATCH. Length and distance follow. Move the state into
        // crazy territory as a reminder to decode a distance later.
        //

        if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
            LZMA_RANGE_UPDATE_0(Prob, ProbValue, Range, Bound);
            State += LZMA_STATE_COUNT;
            Prob = Probs + LzmaProbLengthCoder;

        //
        // Start with 1 + 1.
        //

        } else {
            LZMA_RANGE_UPDATE_1(Prob, ProbValue, Range, Bound, Code);
            if ((CheckDictSize == 0) && (ProcessedPosition == 0)) {
                return LzErrorCorruptData;
            }

            Prob = Probs + LzmaProbIsRepG0 + State;
            LZMA_RANGE_READ(Prob, ProbValue, Bound, Range, Code, Buffer);

            //
            // 1 + 1 + 0 is either a SHORTREP or a LONGREP[0].
            //

            if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                LZMA_RANGE_UPDATE_0(Prob, ProbValue, Range, Bound);
                Prob = Probs + LzmaProbIsRep0Long +
                       (State << LZMA_MAX_POSITION_BITS) + PositionState;

                LZMA_RANGE_READ(Prob, ProbValue, Bound, Range, Code, Buffer);

                //
                // 1 + 1 + 0 + 0 is a SHORTREP, and the symbol is complete.
                //

                if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                    LZMA_RANGE_UPDATE_0(Prob, ProbValue, Range, Bound);
                    DictIndex = DictPosition - Rep0;
                    if (DictPosition < Rep0) {
                        DictIndex += DictBufferSize;
                    }

                    Dict[DictPosition] = Dict[DictIndex];
                    DictPosition += 1;
                    ProcessedPosition += 1;
                    if (State < LZMA_LITERAL_STATE_COUNT) {
                        State = 9;

                    } else {
                        State = 11;
                    }

                    continue;
                }

                //
                // 1 + 1 + 0 + 1 is a LONGREP[0]. Length follows.
                //

                LZMA_RANGE_UPDATE_1(Prob, ProbValue, Range, Bound, Code);

            //
            // 1 + 1 + 1 is a LONGREP of some kind.
            //

            } else {
                LZMA_RANGE_UPDATE_1(Prob, ProbValue, Range, Bound, Code);
                Prob = Probs + LzmaProbIsRepG1 + State;
                LZMA_RANGE_READ(Prob, ProbValue, Bound, Range, Code, Buffer);

                //
                // 1 + 1 + 1 + 0 is LONGREP[1]. Length follows.
                //

                if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                    LZMA_RANGE_UPDATE_0(Prob, ProbValue, Range, Bound);
                    Distance = Rep1;

                //
                // Starts with 1 + 1 + 1 + 1: LONGREP[2] or LONGREP[3].
                //

                } else {
                    LZMA_RANGE_UPDATE_1(Prob, ProbValue, Range, Bound, Code);
                    Prob = Probs + LzmaProbIsRepG2 + State;
                    LZMA_RANGE_READ(Prob,
                                    ProbValue,
                                    Bound,
                                    Range,
                                    Code,
                                    Buffer);

                    //
                    // Four ones and a zero is LONGREP[2]. Length follows.
                    //

                    if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                        LZMA_RANGE_UPDATE_0(Prob, ProbValue, Range, Bound);
                        Distance = Rep2;

                    //
                    // Five ones is a LONGREP[3]. Length follows.
                    //

                    } else {
                        LZMA_RANGE_UPDATE_1(Prob,
                                            ProbValue,
                                            Range,
                                            Bound,
                                            Code);

                        Distance = Rep3;
                        Rep3 = Rep2;
                    }

                    Rep2 = Rep1;
                }

                Rep1 = Rep0;
                Rep0 = Distance;
            }

            if (State < LZMA_LITERAL_STATE_COUNT) {
                State = 8;

            } else {
                State = 11;
            }

            Prob = Probs + LzmaProbRepLengthEncoder;
        }

        //
        // Decode a length. If it starts with a 0, then there are 3 bits
        // decoding lengths from 2 to 9.
        //

        LengthProb = Prob + LzmaLengthChoice;
        LZMA_RANGE_READ(LengthProb, ProbValue, Bound, Range, Code, Buffer);
        if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
            LZMA_RANGE_UPDATE_0(LengthProb, ProbValue, Range, Bound);
            LengthProb = Prob + LzmaLengthLow +
                         (PositionState << LZMA_LENGTH_LOW_BITS);

            Offset = 0;
            LengthLimit = LZMA_LENGTH_LOW_SYMBOLS;

        //
        // The length starts with a 1.
        //

        } else {
            LZMA_RANGE_UPDATE_1(LengthProb, ProbValue, Range, Bound, Code);
            LengthProb = Prob + LzmaLengthChoice2;
            LZMA_RANGE_READ(LengthProb, ProbValue, Bound, Range, Code, Buffer);

            //
            // 1 + 0 follows with 3 bits decoding lengths from 10 to 17.
            //

            if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                LZMA_RANGE_UPDATE_0(LengthProb, ProbValue, Range, Bound);
                LengthProb = Prob + LzmaLengthMid +
                             (PositionState << LZMA_LENGTH_MID_BITS);

                Offset = LZMA_LENGTH_LOW_SYMBOLS;
                LengthLimit = LZMA_LENGTH_MID_SYMBOLS;

            //
            // 1 + 1 follows with 8 bits decoding lengths 18 to 273.
            //

            } else {
                LZMA_RANGE_UPDATE_1(LengthProb, ProbValue, Range, Bound, Code);
                LengthProb = Prob + LzmaLengthHigh;
                Offset = LZMA_LENGTH_LOW_SYMBOLS + LZMA_LENGTH_MID_SYMBOLS;
                LengthLimit = LZMA_LENGTH_HIGH_SYMBOLS;
            }
        }

        //
        // Read out the remaining length bits.
        //

        Length = 0x1;
        do {
            LZMA_RANGE_READ(LengthProb + Length,
                            ProbValue,
                            Bound,
                            Range,
                            Code,
                            Buffer);

            LZMA_RANGE_GET_BIT(LengthProb + Length,
                               ProbValue,
                               Bound,
                               Range,
                               Code,
                               Length);

        } while (Length < LengthLimit);

        Length -= LengthLimit;
        Length += Offset;

        //
        // Decode a distance as well if this is a MATCH. The state was set
        // out of bounds to remember to do this.
        //

        if (State >= LZMA_STATE_COUNT) {
            Prob = Probs + LzmaProbPositionSlot;
            if (Length < LZMA_LENGTH_TO_POSITION_STATES) {
                Prob += Length << LZMA_POSITION_SLOT_BITS;

            } else {
                Prob += (LZMA_LENGTH_TO_POSITION_STATES - 1) <<
                        LZMA_POSITION_SLOT_BITS;
            }

            Distance = 0x1;
            do {
                LZMA_RANGE_READ(Prob + Distance,
                                ProbValue,
                                Bound,
                                Range,
                                Code,
                                Buffer);

                LZMA_RANGE_GET_BIT(Prob + Distance,
                                   ProbValue,
                                   Bound,
                                   Range,
                                   Code,
                                   Distance);

            } while (Distance < LZMA_POSITION_SLOTS);

            Distance -= LZMA_POSITION_SLOTS;
            if (Distance >= LZMA_START_POSITION_MODEL_INDEX) {
                PositionSlot = Distance;
                DirectBits = (Distance >> 1) - 1;
                Distance = (Distance & 0x1) | 0x2;
                if (PositionSlot < LZMA_END_POSITION_MODEL_INDEX) {
                    Distance <<= DirectBits;
                    Prob = Probs + LzmaProbSpecPosition +
                           Distance - PositionSlot - 1;

                    Mask = 1;
                    Symbol = 1;
                    do {
                        LZMA_RANGE_READ(Prob + Symbol,
                                        ProbValue,
                                        Bound,
                                        Range,
                                        Code,
                                        Buffer);

                        if (!LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                            Distance |= Mask;
                        }

                        LZMA_RANGE_GET_BIT(Prob + Symbol,
                                           ProbValue,
                                           Bound,
                                           Range,
                                           Code,
                                           Symbol);

                        Mask <<= 1;
                        DirectBits -= 1;

                    } while (DirectBits != 0);

                } else {
                    DirectBits -= LZMA_ALIGN_TABLE_BITS;
                    do {
                        LZMA_NORMALIZE_RANGE(Range, Code, Buffer);
                        Range >>= 1;
                        Code -= Range;
                        Mask = 0 - ((ULONG)Code >> 31);
                        Distance = (Distance << 1) + (Mask + 1);
                        Code += Range & Mask;
                        DirectBits -= 1;

                    } while (DirectBits != 0);

                    Prob = Probs + LzmaProbAlign;
                    Distance <<= LZMA_ALIGN_TABLE_BITS;
                    Symbol = 1;
                    Mask = 1;
                    for (Bit = 0; Bit < 4; Bit += 1) {
                        LZMA_RANGE_READ(Prob + Symbol,
                                        ProbValue,
                                        Bound,
                                        Range,
                                        Code,
                                        Buffer);

                        if (!LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                            Distance |= Mask;
                        }

                        LZMA_RANGE_GET_BIT(Prob + Symbol,
                                           ProbValue,
                                           Bound,
                                           Range,
                                           Code,
                                           Symbol);

                        Mask <<= 1;
                    }

                    if (Distance == (ULONG)0xFFFFFFFF) {
                        Length += LZMA_MATCH_SPEC_LENGTH_START;
                        State -= LZMA_STATE_COUNT;
                        break;
                    }
                }
            }

            Rep3 = Rep2;
            Rep2 = Rep1;
            Rep1 = Rep0;
            Rep0 = Distance + 1;
            if (CheckDictSize == 0) {
                if (Distance >= ProcessedPosition) {
                    Decoder->DictPosition = DictPosition;
                    return LzErrorCorruptData;
                }

            } else if (Distance >= CheckDictSize) {
                Decoder->DictPosition = DictPosition;
                return LzErrorCorruptData;
            }

            if (State < LZMA_STATE_COUNT + LZMA_LITERAL_STATE_COUNT) {
                State = LZMA_LITERAL_STATE_COUNT;

            } else {
                State = LZMA_LITERAL_STATE_COUNT + 3;
            }
        }

        Length += LZMA_MIN_MATCH_LENGTH;

        //
        // Now that the distance and length of the repetition are known, go
        // grab it from the previous stream.
        //

        Remainder = Limit - DictPosition;
        if (Remainder == 0) {
            Decoder->DictPosition = DictPosition;
            return LzErrorCorruptData;
        }

        CurrentLength = Length;
        if (Remainder < Length) {
            CurrentLength = Remainder;
        }

        Position = DictPosition - Rep0;
        if (DictPosition < Rep0) {
            Position += DictBufferSize;
        }

        ProcessedPosition += CurrentLength;
        Length -= CurrentLength;
        if (CurrentLength <= DictBufferSize - Position) {
            Destination = Dict + DictPosition;
            Source = Position - DictPosition;
            CopyEnd = Destination + CurrentLength;
            DictPosition += CurrentLength;
            do {
                *Destination = (UCHAR)*(Destination + Source);
                Destination += 1;

            } while (Destination != CopyEnd);

        } else {
            do {
                Dict[DictPosition] = Dict[Position];
                DictPosition += 1;
                Position += 1;
                if (Position == DictBufferSize) {
                    Position = 0;
                }

                CurrentLength -= 1;

            } while (CurrentLength != 0);
        }

    } while ((DictPosition < Limit) && (Buffer < BufferEnd));

    LZMA_NORMALIZE_RANGE(Range, Code, Buffer);

    //
    // Put all the toys away.
    //

    Decoder->Buffer = Buffer;
    Decoder->Range = Range;
    Decoder->Code = Code;
    Decoder->RemainingLength = Length;
    Decoder->DictPosition = DictPosition;
    Decoder->ProcessedPosition = ProcessedPosition;
    Decoder->Reps[0] = Rep0;
    Decoder->Reps[1] = Rep1;
    Decoder->Reps[2] = Rep2;
    Decoder->Reps[3] = Rep3;
    Decoder->State = State;
    return LzSuccess;
}

LZMA_DECODE_ATTEMPT
LzpLzmaAttemptDecode (
    PLZMA_DECODER Decoder,
    PCUCHAR Buffer,
    UINTN InSize
    )

/*++

Routine Description:

    This routine attempts to decode a symbol from the output stream, but
    doesn't actually write to the output or update the state machine. It's used
    as a peek function to see if the next symbol is valid.

Arguments:

    Decoder - Supplies a pointer to the decoder.

    Buffer - Supplies a pointer to the input buffer to take a look at.

    InSize - Supplies the number of bytes in the input buffer.

Return Value:

    LZ attempt status, which indicates either what kind of symbol is next or
    that the input is invalid.

--*/

{

    ULONG Bit;
    ULONG Bound;
    ULONG Code;
    UINTN DictIndex;
    ULONG DirectBits;
    PCUCHAR End;
    ULONG Length;
    ULONG LengthLimit;
    PLZ_PROB LengthProb;
    PLZ_PROB LiteralProb;
    ULONG MatchByte;
    ULONG Offset;
    ULONG PositionSlot;
    ULONG PositionState;
    PLZ_PROB Prob;
    PLZ_PROB Probs;
    LZ_PROB ProbValue;
    ULONG Range;
    ULONG State;
    LZMA_DECODE_ATTEMPT Status;
    ULONG Symbol;

    End = Buffer + InSize;
    Code = Decoder->Code;
    Probs = Decoder->Probabilities;
    Range = Decoder->Range;
    State = Decoder->State;

    //
    // Decode a single symbol.
    //

    PositionState = Decoder->ProcessedPosition & ((1 << Decoder->Pb) - 1);
    Prob = Probs + LzmaProbIsMatch + (State << LZMA_MAX_POSITION_BITS) +
           PositionState;

    LZMA_RANGE_READ_ATTEMPT(Prob, ProbValue, Bound, Range, Code, Buffer, End);

    //
    // If the first bit is a zero, decode a literal byte.
    //

    if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
        LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);
        Prob = Probs + LzmaProbLiteral;
        if ((Decoder->CheckDictSize != 0) ||
            (Decoder->ProcessedPosition != 0)) {

            DictIndex = Decoder->DictPosition;
            if (DictIndex == 0) {
                DictIndex = Decoder->DictBufferSize;
            }

            Prob += (ULONG)LZMA_LITERAL_SIZE *
                    (((Decoder->ProcessedPosition &
                       ((1 << Decoder->Lp) - 1)) << Decoder->Lc) +
                     (Decoder->Dict[DictIndex - 1] >> (8 - Decoder->Lc)));
        }

        //
        // Decode a literal byte.
        //

        if (State < LZMA_LITERAL_STATE_COUNT) {
            Symbol = 0x1;
            do {
                LZMA_RANGE_READ_ATTEMPT(Prob + Symbol,
                                        ProbValue,
                                        Bound,
                                        Range,
                                        Code,
                                        Buffer,
                                        End);

                LZMA_RANGE_GET_BIT_ATTEMPT(Bound, Range, Code, Symbol);

            } while (Symbol < 0x100);

        //
        // Decode a matched literal.
        //

        } else {
            DictIndex = Decoder->DictPosition - Decoder->Reps[0];
            if (Decoder->DictPosition < Decoder->Reps[0]) {
                DictIndex += Decoder->DictBufferSize;
            }

            MatchByte = Decoder->Dict[DictIndex];
            Offset = 0x100;
            Symbol = 1;
            do {
                MatchByte <<= 1;
                Bit = MatchByte & Offset;
                LiteralProb = Prob + Offset + Bit + Symbol;
                LZMA_RANGE_READ_ATTEMPT(LiteralProb,
                                        ProbValue,
                                        Bound,
                                        Range,
                                        Code,
                                        Buffer,
                                        End);

                if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                    Offset &= ~Bit;

                } else {
                    Offset &= Bit;
                }

                LZMA_RANGE_GET_BIT_ATTEMPT(Bound, Range, Code, Symbol);

            } while (Symbol < 0x100);
        }

        Status = LzmaAttemptLiteral;
        goto LzmaAttemptDecodeEnd;
    }

    //
    // The symbol starts with at 1, it's a repeat of some kind.
    //

    LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
    Prob = Probs + LzmaProbIsRep + State;
    LZMA_RANGE_READ_ATTEMPT(Prob, ProbValue, Bound, Range, Code, Buffer, End);

    //
    // 1 + 0 is a MATCH. Length and distance to follow.
    //

    if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
        LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);
        State = 0;
        Prob = Probs + LzmaProbLengthCoder;
        Status = LzmaAttemptMatch;

    //
    // 1 + 1 is a REP of some kind.
    //

    } else {
        LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
        Status = LzmaAttemptRep;
        Prob = Probs + LzmaProbIsRepG0 + State;
        LZMA_RANGE_READ_ATTEMPT(Prob,
                                ProbValue,
                                Bound,
                                Range,
                                Code,
                                Buffer,
                                End);

        //
        // 1 + 1 + 0 is either a SHORTREP or a LONGREP[0].
        //

        if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
            LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);
            Prob = Probs + LzmaProbIsRep0Long +
                   (State << LZMA_MAX_POSITION_BITS) + PositionState;

            LZMA_RANGE_READ_ATTEMPT(Prob,
                                    ProbValue,
                                    Bound,
                                    Range,
                                    Code,
                                    Buffer,
                                    End);

            //
            // 1 + 1 + 0 + 0 is a SHORTREP, and no further bits are required.
            //

            if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);
                goto LzmaAttemptDecodeEnd;

            //
            // 1 + 1 + 0 + 1 is a LONGREP[0], length to follow.
            //

            } else {
                LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
            }

        //
        // 1 + 1 + 1 is a LONGREP[1-3]. Length to follow in all cases.
        //

        } else {
            LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
            Prob = Probs + LzmaProbIsRepG1 + State;
            LZMA_RANGE_READ_ATTEMPT(Prob,
                                    ProbValue,
                                    Bound,
                                    Range,
                                    Code,
                                    Buffer,
                                    End);

            //
            // 1 + 1 + 1 + 0 is a LONGREP[1].
            //

            if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);

            //
            // 1 + 1 + 1 + 1 is a LONGREP[2-3].
            //

            } else {
                LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
                Prob = Probs + LzmaProbIsRepG2 + State;
                LZMA_RANGE_READ_ATTEMPT(Prob,
                                        ProbValue,
                                        Bound,
                                        Range,
                                        Code,
                                        Buffer,
                                        End);

                if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
                    LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);

                } else {
                    LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
                }
            }
        }

        State = LZMA_STATE_COUNT;
        Prob = Probs + LzmaProbRepLengthEncoder;
    }

    //
    // Read the length.
    //

    LengthProb = Prob + LzmaLengthChoice;
    LZMA_RANGE_READ_ATTEMPT(LengthProb,
                            ProbValue,
                            Bound,
                            Range,
                            Code,
                            Buffer,
                            End);

    //
    // 0 follows with 3 bits encoding lengths of 2 through 9.
    //

    if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
        LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);
        LengthProb = Prob + LzmaLengthLow +
                     (PositionState << LZMA_LENGTH_LOW_BITS);

        Offset = 0;
        LengthLimit = LZMA_LENGTH_LOW_SYMBOLS;

    } else {
        LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
        LengthProb = Prob + LzmaLengthChoice2;
        LZMA_RANGE_READ_ATTEMPT(LengthProb,
                                ProbValue,
                                Bound,
                                Range,
                                Code,
                                Buffer,
                                End);

        //
        // 1 + 0 follows with 3 bits encoding lengths of 10 through 17.
        //

        if (LZMA_RANGE_IS_BIT_0(Code, Bound)) {
            LZMA_RANGE_UPDATE_0_ATTEMPT(Range, Bound);
            LengthProb = Prob + LzmaLengthMid +
                         (PositionState << LZMA_LENGTH_MID_BITS);

            Offset = LZMA_LENGTH_LOW_SYMBOLS;
            LengthLimit = LZMA_LENGTH_MID_SYMBOLS;

        //
        // 1 + 1 follows with 8 bits encoding lengths of 18 through 273.
        //

        } else {
            LZMA_RANGE_UPDATE_1_ATTEMPT(Range, Bound, Code);
            LengthProb = Prob + LzmaLengthHigh;
            Offset = LZMA_LENGTH_LOW_SYMBOLS + LZMA_LENGTH_MID_SYMBOLS;
            LengthLimit = LZMA_LENGTH_HIGH_SYMBOLS;
        }
    }

    //
    // Read out the remaining length bits.
    //

    Length = 0x1;
    do {
        LZMA_RANGE_READ_ATTEMPT(LengthProb + Length,
                                ProbValue,
                                Bound,
                                Range,
                                Code,
                                Buffer,
                                End);

        LZMA_RANGE_GET_BIT_ATTEMPT(Bound, Range, Code, Length);

    } while (Length < LengthLimit);

    Length -= LengthLimit;
    Length += Offset;

    //
    // Decode a distance as well if this was a MATCH.
    //

    if (State < 4) {
        Prob = Probs + LzmaProbPositionSlot;
        if (Length < LZMA_LENGTH_TO_POSITION_STATES) {
            Prob += Length << LZMA_POSITION_SLOT_BITS;

        } else {
            Prob += (LZMA_LENGTH_TO_POSITION_STATES - 1) <<
                    LZMA_POSITION_SLOT_BITS;
        }

        PositionSlot = 1;
        do {
            LZMA_RANGE_READ_ATTEMPT(Prob + PositionSlot,
                                    ProbValue,
                                    Bound,
                                    Range,
                                    Code,
                                    Buffer,
                                    End);

            LZMA_RANGE_GET_BIT_ATTEMPT(Bound, Range, Code, PositionSlot);

        } while (PositionSlot < LZMA_POSITION_SLOTS);

        PositionSlot -= LZMA_POSITION_SLOTS;
        if (PositionSlot >= LZMA_START_POSITION_MODEL_INDEX) {
            DirectBits = (PositionSlot >> 1) - 1;
            if (PositionSlot < LZMA_END_POSITION_MODEL_INDEX) {
                Prob = Probs + LzmaProbSpecPosition +
                       (((PositionSlot & 0x1) | 0x2) << DirectBits) -
                       PositionSlot - 1;

            } else {
                DirectBits -= LZMA_ALIGN_TABLE_BITS;
                do {
                    LZMA_NORMALIZE_RANGE_ATTEMPT(Range, Code, Buffer, End);
                    Range >>= 1;

                    //
                    // If the code is greater than the range, subtract the
                    // range from code.
                    //

                    Code -= Range & (((Code - Range) >> 31) - 1);
                    DirectBits -= 1;

                } while (DirectBits != 0);

                Prob = Probs + LzmaProbAlign;
                DirectBits = LZMA_ALIGN_TABLE_BITS;
            }

            Symbol = 0x1;
            do {
                LZMA_RANGE_READ_ATTEMPT(Prob + Symbol,
                                        ProbValue,
                                        Bound,
                                        Range,
                                        Code,
                                        Buffer,
                                        End);

                LZMA_RANGE_GET_BIT_ATTEMPT(Bound, Range, Code, Symbol);
                DirectBits -= 1;

            } while (DirectBits != 0);
        }
    }

LzmaAttemptDecodeEnd:
    LZMA_NORMALIZE_RANGE_ATTEMPT(Range, Code, Buffer, End);
    return Status;
}

VOID
LzpLzmaDecoderWriteRemainder (
    PLZMA_DECODER Decoder,
    UINTN Limit
    )

/*++

Routine Description:

    This routine writes out the remaining bytes in the decoder based on the
    last REP.

Arguments:

    Decoder - Supplies a pointer to the decoder.

    Limit - Supplies the number of bytes.

Return Value:

    None.

--*/

{

    PUCHAR Dict;
    UINTN DictBufferSize;
    UINTN DictIndex;
    UINTN DictPosition;
    ULONG Length;
    UINTN Remainder;
    UINTN Rep0;

    if ((Decoder->RemainingLength == 0) ||
        (Decoder->RemainingLength >= LZMA_MATCH_SPEC_LENGTH_START)) {

        return;
    }

    Dict = Decoder->Dict;
    DictPosition = Decoder->DictPosition;
    DictBufferSize = Decoder->DictBufferSize;
    Length = Decoder->RemainingLength;
    Remainder = Limit - DictPosition;
    Rep0 = Decoder->Reps[0];
    if (Remainder < Length) {
        Length = (ULONG)Remainder;
    }

    if ((Decoder->CheckDictSize == 0) &&
        (Decoder->DictSize - Decoder->ProcessedPosition <= Length)) {

        Decoder->CheckDictSize = Decoder->DictSize;
    }

    Decoder->ProcessedPosition += Length;
    Decoder->RemainingLength -= Length;
    while (Length != 0) {
        Length -= 1;
        DictIndex = DictPosition - Rep0;
        if (DictPosition < Rep0) {
            DictIndex += DictBufferSize;
        }

        Dict[DictPosition] = Dict[DictIndex];
        DictPosition += 1;
    }

    Decoder->DictPosition = DictPosition;
    return;
}


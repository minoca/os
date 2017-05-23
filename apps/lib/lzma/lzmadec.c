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

    Stage - Stores the stage of decoding.

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

    AllocatedInput - Stores a pointer to an allocated input buffer, if no
        direct input buffer is being provided.

    AllocatedOutput - Stores a pointer to an allocated output buffer, if no
        direct output buffer is being provided.

    InputPosition - Stores the position within the input buffer of the first
        unprocessed byte.

    InputSize - Stores the number of valid bytes in the input buffer.

    OutputPosition - Stores the position within the output buffer to write the
        next byte.

    FileWrapper - Stores a boolean indicating if the file footer is expected.

    HasEndMark - Stores a boolean indicating if the stream has an end mark.

    InputFinished - Stores a boolean indicating whether this is the end of
        the input or not.

    Error - Stores the error that occurred during decoding.

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
    LZMA_STAGE Stage;
    ULONG Reps[LZMA_REP_COUNT];
    ULONG RemainingLength;
    BOOL NeedFlush;
    BOOL NeedReset;
    ULONG ProbabilityCount;
    ULONG WorkingSize;
    UCHAR Working[LZMA_MAX_INPUT];
    PVOID AllocatedInput;
    PVOID AllocatedOutput;
    ULONG InputPosition;
    ULONG InputSize;
    ULONG OutputPosition;
    BOOL FileWrapper;
    BOOL HasEndMark;
    BOOL InputFinished;
    LZ_STATUS Error;
} LZMA_DECODER, *PLZMA_DECODER;

//
// ----------------------------------------------- Internal Function Prototypes
//

PLZMA_DECODER
LzpLzmaDecoderCreate (
    PLZ_CONTEXT Context
    );

LZ_STATUS
LzpLzmaDecoderInitialize (
    PLZ_CONTEXT Context,
    PLZMA_ENCODER_PROPERTIES Properties
    );

VOID
LzpLzmaDecoderDestroy (
    PLZ_CONTEXT Context,
    PLZMA_DECODER Decoder
    );

LZ_STATUS
LzpLzmaDecodeProperties (
    PLZ_CONTEXT Context,
    PCUCHAR Properties,
    ULONG PropertiesSize
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
LzpLzmaReadHeader (
    PLZ_CONTEXT Context
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

LZ_STATUS
LzpVerifyCheckFields (
    UCHAR CheckFields[LZMA_FOOTER_SIZE],
    PLZ_CONTEXT Context
    );

LZ_STATUS
LzpLzmaDecoderRead (
    PLZ_CONTEXT Context,
    PUCHAR Buffer,
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LZ_STATUS
LzLzmaInitializeDecoder (
    PLZ_CONTEXT Context,
    PLZMA_ENCODER_PROPERTIES Properties,
    BOOL FileWrapper
    )

/*++

Routine Description:

    This routine initializes an LZMA context for decoding.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

    Properties - Supplies an optional pointer to the properties used in the
        upcoming encoding stream. If this is NULL, default properties
        equivalent to an encoding level of five will be set.

    FileWrapper - Supplies a boolean indicating if the file header and footer
        should be expected from the input stream.

Return Value:

    Returns an LZ status code indicating overall success or failure.

--*/

{

    PLZMA_DECODER Decoder;
    BOOL DecoderAllocated;
    LZ_STATUS Status;

    DecoderAllocated = FALSE;
    if (Context->InternalState == NULL) {
        Decoder = LzpLzmaDecoderCreate(Context);
        if (Decoder == NULL) {
            return LzErrorMemory;
        }

        Context->InternalState = Decoder;
        DecoderAllocated = TRUE;

    } else {
        Decoder = Context->InternalState;
    }

    Decoder->FileWrapper = FileWrapper;
    if (FileWrapper != FALSE) {
        Decoder->Stage = LzmaStageFileHeader;

    } else {
        Decoder->Stage = LzmaStageData;
    }

    if (Properties != NULL) {
        Status = LzpLzmaDecoderInitialize(Context, Properties);
        if (Status != LzSuccess) {
            goto InitializeDecoderEnd;
        }
    }

    Status = LzSuccess;

InitializeDecoderEnd:
    if (Status != LzSuccess) {
        if (DecoderAllocated != FALSE) {
            LzpLzmaDecoderDestroy(Context, Decoder);
            Context->InternalState = NULL;
        }
    }

    return Status;
}

LZ_STATUS
LzLzmaDecode (
    PLZ_CONTEXT Context,
    LZ_FLUSH_OPTION Flush
    )

/*++

Routine Description:

    This routine decompresses an LZMA stream.

Arguments:

    Context - Supplies a pointer to the context.

    Flush - Supplies the flush option, which indicates whether the decoder
        should be flushed and terminated with this call or not.

Return Value:

    LZ Status code.

--*/

{

    INTN BytesComplete;
    UCHAR CheckBuffer[LZMA_FOOTER_SIZE];
    LZ_COMPLETION_STATUS CompletionStatus;
    PLZMA_DECODER Decoder;
    BOOL EndMark;
    PUCHAR InBuffer;
    UINTN InBufferSize;
    UINTN InPosition;
    UINTN InProcessed;
    UINTN InSize;
    UINTN OriginalBufferSize;
    PVOID OriginalDict;
    PUCHAR OutBuffer;
    UINTN OutBufferSize;
    UINTN OutPosition;
    UINTN OutProcessed;
    LZ_STATUS Status;

    Decoder = Context->InternalState;

    //
    // Remember any errors that had occurred previously, like a broken record.
    //

    Status = Decoder->Error;
    if (Status != LzSuccess) {
        goto DecodeEnd;
    }

    //
    // Decode the file header, which may advance on to the data stage, or may
    // not.
    //

    if (Decoder->Stage == LzmaStageFileHeader) {
        Status = LzpLzmaReadHeader(Context);
        if (Status != LzSuccess) {
            goto DecodeEnd;
        }

        //
        // Specifically bail out now if reading the header used up all the
        // input data. There's detection below if no progress was made, and
        // that would fire in this case.
        //

        if ((Context->Read == NULL) && (Context->InputSize == 0)) {
            goto DecodeEnd;
        }
    }

    //
    // The meat is here, decoding data.
    //

    if (Decoder->Stage == LzmaStageData) {

        //
        // Figure out which buffers to use: previously allocated internal
        // buffers or the buffers coming in direct from the user.
        //

        if (Context->Read != NULL) {
            InBuffer = Decoder->AllocatedInput;
            InBufferSize = LZMA_DECODE_DEFAULT_WORKING_SIZE;
            InSize = Decoder->InputSize;
            InPosition = Decoder->InputPosition;

        } else {
            InBuffer = (PUCHAR)(Context->Input);
            InBufferSize = Context->InputSize;
            InSize = Context->InputSize;
            InPosition = 0;
        }

        if (Context->Write != NULL) {
            OutBuffer = Decoder->AllocatedOutput;
            OutPosition = Decoder->OutputPosition;
            OutBufferSize = LZMA_DECODE_DEFAULT_WORKING_SIZE;

        } else {
            OutBuffer = Context->Output;
            OutPosition = 0;
            OutBufferSize = Context->OutputSize;
        }

        //
        // If using the buffer, decode directly to the output buffer, since
        // there's only one shot.
        //

        CompletionStatus = LzCompletionNotSpecified;
        if ((Context->Write == NULL) && (Context->Read == NULL) &&
            (Flush == LzFlushNow)) {

            OriginalDict = Decoder->Dict;
            OriginalBufferSize = Decoder->DictBufferSize;
            Decoder->Dict = Context->Output;
            Decoder->DictBufferSize = Context->OutputSize;
            Status = LzpLzmaDecodeToDictionary(Decoder,
                                               Decoder->DictBufferSize,
                                               Context->Input,
                                               &InSize,
                                               Decoder->HasEndMark,
                                               &CompletionStatus);

            Decoder->Dict = OriginalDict;
            Decoder->DictBufferSize = OriginalBufferSize;
            Context->CompressedCrc32 = LzpComputeCrc32(Context->CompressedCrc32,
                                                       Context->Input,
                                                       InSize);

            Context->CompressedSize += InSize;
            Context->Input += InSize;
            Context->InputSize -= InSize;
            Context->UncompressedCrc32 =
                                    LzpComputeCrc32(Context->UncompressedCrc32,
                                                    Context->Output,
                                                    Decoder->DictPosition);

            Context->UncompressedSize += Decoder->DictPosition;
            Context->Output += Decoder->DictPosition;
            Context->OutputSize -= Decoder->DictPosition;
            if ((Status == LzSuccess) &&
                ((CompletionStatus == LzCompletionMoreInputRequired) ||
                 (CompletionStatus == LzCompletionNotFinished))) {

                Status = LzErrorInputEof;
            }

            if (Status != LzSuccess) {
                goto DecodeEnd;
            }

        } else {

            //
            // This is the normal loop for decoding the stream.
            //

            while (TRUE) {
                EndMark = FALSE;
                if (InPosition >= InSize) {
                    if (Context->Read == NULL) {
                        if (Flush != LzNoFlush) {
                            EndMark = Decoder->HasEndMark;

                        } else {
                            break;
                        }

                    } else if (Decoder->InputFinished == FALSE) {
                        InSize = Context->Read(Context, InBuffer, InBufferSize);
                        if (InSize == 0) {
                            Decoder->InputFinished = TRUE;
                            EndMark = Decoder->HasEndMark;

                        } else if (InSize < 0) {
                            Status = LzErrorRead;
                            break;
                        }

                        InPosition = 0;

                    } else {
                        EndMark = Decoder->HasEndMark;
                    }
                }

                InProcessed = InSize - InPosition;
                OutProcessed = OutBufferSize - OutPosition;
                if (OutProcessed == 0) {
                    Status = LzSuccess;
                    break;
                }

                Status = LzpLzmaDecodeToBuffer(Decoder,
                                               OutBuffer + OutPosition,
                                               &OutProcessed,
                                               InBuffer + InPosition,
                                               &InProcessed,
                                               EndMark,
                                               &CompletionStatus);

                //
                // Only add the portion of the buffer that's actually compressed
                // data to the CRC.
                //

                Context->CompressedCrc32 =
                                      LzpComputeCrc32(Context->CompressedCrc32,
                                                      InBuffer + InPosition,
                                                      InProcessed);

                Context->CompressedSize += InProcessed;
                InPosition += InProcessed;
                OutPosition += OutProcessed;
                if (Context->Write != NULL) {
                    BytesComplete = Context->Write(Context,
                                                   OutBuffer,
                                                   OutPosition);

                    if (BytesComplete != OutPosition) {
                        Status = LzErrorWrite;
                        break;
                    }
                }

                Context->UncompressedCrc32 =
                                    LzpComputeCrc32(Context->UncompressedCrc32,
                                                    OutBuffer,
                                                    OutPosition);

                Context->UncompressedSize += OutPosition;
                if (Context->Write != NULL) {
                    OutPosition = 0;
                }

                if (Status != LzSuccess) {
                    break;
                }

                if (CompletionStatus == LzCompletionFinishedWithMark) {
                    break;
                }

                if ((InProcessed == 0) && (OutProcessed == 0)) {
                    break;
                }
            }

            if (Context->Read != NULL) {
                Decoder->InputSize = InSize;
                Decoder->InputPosition = InPosition;

            } else {
                Context->Input += InPosition;
                Context->InputSize -= InPosition;
                Context->Output += OutPosition;
                Context->OutputSize -= OutPosition;
            }

            if (Status != LzSuccess) {
                goto DecodeEnd;
            }
        }

        //
        // See if the stream is complete, and advance the stage if so.
        //

        if ((CompletionStatus == LzCompletionFinishedWithMark) ||
            ((Decoder->HasEndMark == FALSE) &&
             (Flush != LzNoFlush) &&
             (CompletionStatus == LzCompletionMaybeFinishedWithoutMark))) {

            if (Decoder->FileWrapper != FALSE) {
                Decoder->Stage = LzmaStageFileFooter;

            } else {
                Decoder->Stage = LzmaStageComplete;
            }
        }

        //
        // If the stream was supposed to finish but didn't, that's an error.
        //

        if ((Flush == LzFlushNow) && (Decoder->Stage == LzmaStageData)) {
            Status = LzErrorInputEof;
            goto DecodeEnd;
        }

        //
        // If no progress was made, fail. If the caller wanted to flush, make
        // the error permanent.
        //

        if ((InBuffer == Context->Input) && (OutBuffer == Context->Output)) {
            Status = LzErrorInvalidParameter;
            if (Flush == LzNoFlush) {
                return Status;
            }

            if (CompletionStatus == LzCompletionMoreInputRequired) {
                Status = LzErrorInputEof;
            }

            goto DecodeEnd;
        }
    }

    //
    // If the stage is now on the footer, read and process that.
    //

    if (Decoder->Stage == LzmaStageFileFooter) {
        Status = LzpLzmaDecoderRead(Context, CheckBuffer, sizeof(CheckBuffer));
        if (Status != LzSuccess) {
            if (Status == LzErrorProgress) {
                Status = LzSuccess;
            }

            goto DecodeEnd;
        }

        Status = LzpVerifyCheckFields(CheckBuffer, Context);
        Decoder->Stage = LzmaStageComplete;
        if (Status != LzSuccess) {
            goto DecodeEnd;
        }
    }

    //
    // If done, then hooray.
    //

    if (Decoder->Stage == LzmaStageComplete) {
        Status = LzStreamComplete;
    }

DecodeEnd:
    if (Status != LzSuccess) {
        Decoder->Error = Status;
    }

    return Status;
}

VOID
LzLzmaFinishDecode (
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine discards any pending input and output data, and tears down
    all allocations associated with the given decoder.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

Return Value:

    None.

--*/

{

    if (Context->InternalState != NULL) {
        LzpLzmaDecoderDestroy(Context, Context->InternalState);
        Context->InternalState = NULL;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PLZMA_DECODER
LzpLzmaDecoderCreate (
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine allocates and initializes the LZMA decoder.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

Return Value:

    Returns a pointer to the LZMA decoder on success.

    NULL on allocation failure.

--*/

{

    PLZMA_DECODER Decoder;

    LzpCrcInitialize();
    Decoder = Context->Reallocate(NULL, sizeof(LZMA_DECODER));
    if (Decoder == NULL) {
        return NULL;
    }

    memset(Decoder, 0, sizeof(LZMA_DECODER));
    if (Context->Read != NULL) {
        Decoder->AllocatedInput =
                   Context->Reallocate(NULL, LZMA_DECODE_DEFAULT_WORKING_SIZE);

        if (Decoder->AllocatedInput == NULL) {
            LzpLzmaDecoderDestroy(Context, Decoder);
            return NULL;
        }
    }

    if (Context->Write != NULL) {
        Decoder->AllocatedOutput =
                   Context->Reallocate(NULL, LZMA_DECODE_DEFAULT_WORKING_SIZE);

        if (Decoder->AllocatedOutput == NULL) {
            LzpLzmaDecoderDestroy(Context, Decoder);
            return NULL;
        }
    }

    return Decoder;
}

LZ_STATUS
LzpLzmaDecoderInitialize (
    PLZ_CONTEXT Context,
    PLZMA_ENCODER_PROPERTIES Properties
    )

/*++

Routine Description:

    This routine initializes the LZMA decoder based on the given properties.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

    Properties - Supplies a pointer to the properties to set up for.

Return Value:

    LZ status code.

--*/

{

    PLZMA_DECODER Decoder;
    UINTN DictBufferSize;
    ULONG DictSize;
    ULONG Mask;
    ULONG ProbabilitiesCount;

    Decoder = Context->InternalState;
    Context->CompressedCrc32 = 0;
    Context->UncompressedCrc32 = 0;
    Context->CompressedSize = 0;
    Context->UncompressedSize = 0;
    Decoder->DictSize = Properties->DictionarySize;
    if (Decoder->DictSize < LZMA_MINIMUM_DICT_SIZE) {
        Decoder->DictSize = LZMA_MINIMUM_DICT_SIZE;
    }

    Decoder->Lc = Properties->Lc;
    Decoder->Pb = Properties->Pb;
    Decoder->Lp = Properties->Lp;
    ProbabilitiesCount = LZMA_PROBABILITIES_COUNT(Decoder);
    Decoder->Probabilities =
                     Context->Reallocate(Decoder->Probabilities,
                                         ProbabilitiesCount * sizeof(LZ_PROB));

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

    Decoder->Dict = Context->Reallocate(Decoder->Dict, DictBufferSize);
    if (Decoder->Dict == NULL) {
        return LzErrorMemory;
    }

    Decoder->DictBufferSize = DictBufferSize;
    Decoder->HasEndMark = Properties->EndMark;
    Decoder->Error = LzSuccess;
    LzpLzmaDecoderReset(Decoder);
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

    if (Decoder->AllocatedInput != NULL) {
        Context->Reallocate(Decoder->AllocatedInput, 0);
        Decoder->AllocatedInput = NULL;
    }

    if (Decoder->AllocatedOutput != NULL) {
        Context->Reallocate(Decoder->AllocatedOutput, 0);
        Decoder->AllocatedOutput = NULL;
    }

    Context->Reallocate(Decoder, 0);
    return;
}

LZ_STATUS
LzpLzmaDecodeProperties (
    PLZ_CONTEXT Context,
    PCUCHAR PropertiesBuffer,
    ULONG PropertiesSize
    )

/*++

Routine Description:

    This routine decodes the LZMA properties bytes and initializes the decoder
    with them.

Arguments:

    Context - Supplies a pointer to the decoding context.

    PropertiesBuffer - Supplies a pointer to the properties bytes.

    PropertiesSize - Supplies the number of properties bytes in byte given
        buffer.

    Decoder - Supplies a pointer to the decoder to initialize.

Return Value:

    LZ Status code.

--*/

{

    UCHAR Parameters;
    LZMA_ENCODER_PROPERTIES Properties;
    LZ_STATUS Status;

    if (PropertiesSize < LZMA_PROPERTIES_SIZE) {
        return LzErrorUnsupported;
    }

    memset(&Properties, 0, sizeof(Properties));
    Properties.DictionarySize = PropertiesBuffer[1] |
                                ((ULONG)(PropertiesBuffer[2]) << 8) |
                                ((ULONG)(PropertiesBuffer[3]) << 16) |
                                ((ULONG)(PropertiesBuffer[4]) << 24);

    if (Properties.DictionarySize < LZMA_MINIMUM_DICT_SIZE) {
        Properties.DictionarySize = LZMA_MINIMUM_DICT_SIZE;
    }

    Parameters = PropertiesBuffer[0];
    if (Parameters >= (9 * 5 * 5)) {
        return LzErrorUnsupported;
    }

    Properties.Lc = Parameters % 9;
    Parameters /= 9;
    Properties.Pb = Parameters / 5;
    Properties.Lp = Parameters % 5;
    Properties.EndMark = TRUE;
    Status = LzpLzmaDecoderInitialize(Context, &Properties);
    return Status;
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
    Decoder->InputFinished = FALSE;
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
LzpLzmaReadHeader (
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads the file header out of a compressed LZMA stream, which
    validates the magic value and reads the properties into the decoder.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    LZ Status code.

--*/

{

    PLZMA_DECODER Decoder;
    UCHAR Header[LZMA_HEADER_SIZE];
    ULONG Magic;
    LZ_STATUS Status;

    Decoder = Context->InternalState;

    //
    // Read both the magic and the properties.
    //

    Status = LzpLzmaDecoderRead(Context, Header, sizeof(Header));

    //
    // If progress is returned, that's a wink that nothing went wrong but the
    // full size isn't there yet.
    //

    if (Status == LzErrorProgress) {
        return LzSuccess;
    }

    if (Status != LzSuccess) {
        return Status;
    }

    memcpy(&Magic, Header, sizeof(Magic));

    //
    // Validate the magic value.
    //

    if (Magic != LZMA_HEADER_MAGIC) {
        return LzErrorMagic;
    }

    Status = LzpLzmaDecodeProperties(Context,
                                     &(Header[LZMA_HEADER_MAGIC_SIZE]),
                                     LZMA_HEADER_SIZE - LZMA_HEADER_MAGIC_SIZE);

    if (Status != LzSuccess) {
        return Status;
    }

    Context->CompressedCrc32 = LzpComputeCrc32(Context->CompressedCrc32,
                                               Header,
                                               LZMA_HEADER_SIZE);

    Context->CompressedSize += LZMA_HEADER_SIZE;

    //
    // Advance to the data portion.
    //

    Decoder->Stage = LzmaStageData;
    return Status;
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

LZ_STATUS
LzpVerifyCheckFields (
    UCHAR CheckFields[LZMA_FOOTER_SIZE],
    PLZ_CONTEXT Context
    )

/*++

Routine Description:

    This routine verifies the check fields at the end of a stream.

Arguments:

    CheckFields - Supplies the buffer containing the uncompressed length,
        compressed CRC32 and uncompressed CRC32.

    Context - Supplies the context containing the computed values.

Return Value:

    LZ Status Code.

--*/

{

    ULONG CompressedCrc32;
    ULONG UncompressedCrc32;
    ULONGLONG UncompressedSize;

    memcpy(&UncompressedSize, &(CheckFields[0]), sizeof(ULONGLONG));
    memcpy(&CompressedCrc32, &(CheckFields[8]), sizeof(ULONG));
    memcpy(&UncompressedCrc32, &(CheckFields[12]), sizeof(ULONG));
    if ((UncompressedSize != Context->UncompressedSize) ||
        (CompressedCrc32 != Context->CompressedCrc32) ||
        (UncompressedCrc32 != Context->UncompressedCrc32)) {

        return LzErrorCrc;
    }

    return LzSuccess;
}

LZ_STATUS
LzpLzmaDecoderRead (
    PLZ_CONTEXT Context,
    PUCHAR Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine reads the specified number of bytes from the input.

Arguments:

    Context - Supplies a pointer to the context.

    Buffer - Supplies a pointer where the read data will be returned on success.

    Size - Supplies the size to read.

Return Value:

    LZ Status code. Returns LzErrorProgress to indicate that not enough data
    is available yet.

--*/

{

    INTN BytesRead;
    UINTN CopySize;
    PLZMA_DECODER Decoder;
    UINTN InBufferSize;
    UINTN InPosition;
    PUCHAR Input;
    UINTN InSize;
    LZ_STATUS Status;

    if (Size > LZMA_MAX_INPUT) {
        return LzErrorInvalidParameter;
    }

    Decoder = Context->InternalState;

    //
    // Decide what buffer is going to be read from.
    //

    if (Context->Read != NULL) {
        Input = Decoder->AllocatedInput;
        InBufferSize = LZMA_DECODE_DEFAULT_WORKING_SIZE;
        InSize = Decoder->InputSize;
        InPosition = Decoder->InputPosition;

    } else {
        Input = (PUCHAR)(Context->Input);
        InBufferSize = 0;
        InPosition = 0;
        InSize = Context->InputSize;
    }

    //
    // Try to repeatedly get the working buffer full.
    //

    Status = LzErrorProgress;
    while (Decoder->WorkingSize < Size) {

        //
        // Read buffer from the input if available. This is either the
        // allocated buffer used to accept read data or the buffer directly
        // from the user.
        //

        if (InPosition < InSize) {
            CopySize = InSize - InPosition;
            if (CopySize > Size - Decoder->WorkingSize) {
                CopySize = Size - Decoder->WorkingSize;
            }

            memcpy(Decoder->Working + Decoder->WorkingSize,
                   Input + InPosition,
                   CopySize);

            Decoder->WorkingSize += CopySize;
            if (Context->Read == NULL) {
                Input += CopySize;
                InSize -= CopySize;

            } else {
                InPosition += CopySize;
            }

            //
            // Break out if finished.
            //

            if (Decoder->WorkingSize >= Size) {
                Status = LzSuccess;
                break;
            }
        }

        //
        // If there's no way to get more data, then there's nothing more that
        // can be done now.
        //

        if ((Context->Read == NULL) || (Decoder->InputFinished != FALSE)) {
            break;
        }

        //
        // Read some new data.
        //

        InPosition = 0;
        InSize = 0;
        BytesRead = Context->Read(Context, Input, InBufferSize);
        if (BytesRead <= 0) {
            if (BytesRead == 0) {
                Decoder->InputFinished = TRUE;
                Status = LzErrorInputEof;
                break;
            }

            Status = LzErrorRead;
            break;
        }

        InSize = BytesRead;
    }

    if (Context->Read != NULL) {
        Decoder->InputPosition = InPosition;
        Decoder->InputSize = InSize;

    } else {
        Context->Input = Input;
        Context->InputSize = InSize;
    }

    if (Status != LzSuccess) {
        return Status;
    }

    //
    // The data is all here. Hand it to the caller. If there's too much data
    // in the working buffer, scootch it down.
    //

    memcpy(Buffer, Decoder->Working, Size);
    if (Decoder->WorkingSize > Size) {
        for (InPosition = 0;
             InPosition < Decoder->WorkingSize - Size;
             InPosition += 1) {

            Decoder->Working[InPosition] = Decoder->Working[InPosition + Size];
        }
    }

    Decoder->WorkingSize -= Size;
    return LzSuccess;
}


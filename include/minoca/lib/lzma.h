/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzma.h

Abstract:

    This header contains definitions for the LZMA library, a port of Igor
    Pavlov's 7zip compression system.

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

//
// Define the smallest supported dictionary size.
//

#define LZMA_MINIMUM_DICT_SIZE (1 << 12)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _LZ_STATUS {
    LzSuccess,
    LzErrorCorruptData,
    LzErrorMemory,
    LzErrorCrc,
    LzErrorUnsupported,
    LzErrorInvalidParameter,
    LzErrorInputEof,
    LzErrorOutputEof,
    LzErrorRead,
    LzErrorWrite,
    LzErrorProgress
} LZ_STATUS, *PLZ_STATUS;

//
// LZMA decoder completion status, indicating whether the stream has ended or
// whether more data is expected.
//

typedef enum _LZ_COMPLETION_STATUS {
    LzCompletionNotSpecified,
    LzCompletionFinishedWithMark,
    LzCompletionNotFinished,
    LzCompletionMoreInputRequired,
    LzCompletionMaybeFinishedWithoutMark
} LZ_COMPLETION_STATUS, *PLZ_COMPLETION_STATUS;

typedef struct _LZ_CONTEXT LZ_CONTEXT, *PLZ_CONTEXT;

typedef
PVOID
(*PLZ_REALLOCATE) (
    PVOID Allocation,
    UINTN NewSize
    );

/*++

Routine Description:

    This routine represents the prototype of the function called when the LZMA
    library needs to allocate, reallocate, or free memory.

Arguments:

    Allocation - Supplies an optional pointer to the allocation to resize or
        free. If NULL, then this routine will allocate new memory.

    NewSize - Supplies the size of the desired allocation. If this is 0 and the
        allocation parameter is non-null, the given allocation will be freed.
        Otherwise it will be resized to requested size.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure, or in the case the memory is being freed.

--*/

typedef
LZ_STATUS
(*PLZ_REPORT_PROGRESS) (
    PLZ_CONTEXT Context,
    ULONGLONG InputSize,
    ULONGLONG OutputSize
    );

/*++

Routine Description:

    This routine represents the prototype of the function called when the LZMA
    library is reporting a progress update.

Arguments:

    Context - Supplies a pointer to the LZ context.

    InputSize - Supplies the number of input bytes processed. Set to -1ULL if
        unknown.

    OutputSize - Supplies the number of output bytes processed. Set to -1ULL if
        unknown.

Return Value:

    0 on success.

    Returns a non-zero value to cancel the operation.

--*/

typedef
INTN
(*PLZ_PERFORM_IO) (
    PLZ_CONTEXT Context,
    PVOID Buffer,
    UINTN Size
    );

/*++

Routine Description:

    This routine represents the prototype of the function called to read
    or write data.

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

/*++

Structure Description:

    This structure stores the general LZ library context.

Members:

    Context - Stores an unused context pointer that the environment integrating
        the LZMA library can use for its own purposes.

    Reallocate - Stores a pointer to a function used to allocate, reallocate,
        and free memory.

    ReportProgress - Stores an optional pointer to a function used to report
        progress updates to the surrounding environment.

    Read - Stores a pointer to a function used to read input data.

    Write - Stores a pointer to a function used to write output data.

    ReadContext - Stores an unused context pointer available for use by the
        surrounding application. This often stores the input file information.

    WriteContext - Stores an unused context pointer available for use by the
        surrounding application. This often sotres the output file information.

--*/

struct _LZ_CONTEXT {
    PVOID Context;
    PLZ_REALLOCATE Reallocate;
    PLZ_REPORT_PROGRESS ReportProgress;
    PLZ_PERFORM_IO Read;
    PLZ_PERFORM_IO Write;
    PVOID ReadContext;
    PVOID WriteContext;
};

/*++

Structure Description:

    This structure stores the LZMA encoder properties.

Members:

    Level - Stores the encoding level. Valid values are 0 through 9,
        inclusive.

    DictionarySize - Stores the dictionary size. Valid values are powers of
        two between (1 << 12) and (1 << 27) inclusive for the 32-bit version,
        or between (1 << 12) and (1 << 30) inclusive for the 64-bit version.
        The default is (1 << 24).

    ReduceSize - Stores the estimated size of the data that will be
        compressed. The default is -1ULL. The encoder may use this value to
        reduce the dictionary size.

    Lc - Stores the lc parameter. Valid values are between 0 and 8, inclusive.
        The default is 3.

    Lp - Stores the lp parameter. Valid values are between 0 and 4, inclusive.
        The default is 0.

    Pb - Stores the pb parameter. Valid values are between 0 and 4, inclusive.
        The default is 2.

    Algorithm - Stores the algorithm speed. Set to 1 for normal, which is the
        default. Set to zero for fast mode.

    FastBytes - Stores the fast byte count parameter. Valid values are between
        5 and 273, inclusive. The default is 32.

    BinTreeMode - Stores a boolean indicating whether to run in bin tree mode
        (TRUE, the default), or hash chain mode (FALSE).

    HashByteCount - Stores the number of bytes in a hash. Valid values are 2
        through 4, inclusive. The default is 4.

    MatchCount - Stores the number of match finder cycles. Valid values are
        between 1 and (1 << 30), inclusive. The default is 32.

    WriteEndMark - Stores a boolean indicating whether to write and end marker
        or not. The default is FALSE.

    ThreadCount - Stores the thread count to use while encoding. Valid values
        are 1 and 2. The default is 2.

--*/

typedef struct _LZMA_ENCODER_PROPERTIES {
    INT Level;
    ULONG DictionarySize;
    ULONGLONG ReduceSize;
    INT Lc;
    INT Lp;
    INT Pb;
    INT Algorithm;
    INT FastBytes;
    INT BinTreeMode;
    INT HashByteCount;
    ULONG MatchCount;
    BOOL WriteEndMark;
    INT ThreadCount;
} LZMA_ENCODER_PROPERTIES, *PLZMA_ENCODER_PROPERTIES;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
LzLzmaEncoderInitializeProperties (
    PLZMA_ENCODER_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine initializes LZMA encoder properties to their defaults.

Arguments:

    Properties - Supplies a pointer to the properties to initialize.

Return Value:

    None.

--*/

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
    );

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

LZ_STATUS
LzLzmaEncodeStream (
    PLZMA_ENCODER_PROPERTIES Properties,
    PLZ_CONTEXT Context
    );

/*++

Routine Description:

    This routine LZMA encodes the given stream.

Arguments:

    Properties - Supplies the encoder properties to set.

    Context - Supplies a pointer to the general LZ context.

Return Value:

    LZ status.

--*/

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
    );

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

LZ_STATUS
LzLzmaDecodeStream (
    PLZ_CONTEXT Context,
    PVOID InBuffer,
    UINTN InBufferSize,
    PVOID OutBuffer,
    UINTN OutBufferSize
    );

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


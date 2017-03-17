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
// Define the magic value at the top of the file format.
//

#define LZMA_HEADER_MAGIC 0x414D5A4C
#define LZMA_HEADER_MAGIC_SIZE 4

#define LZMA_HEADER_SIZE (LZMA_HEADER_MAGIC_SIZE + LZMA_PROPERTIES_SIZE)

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
    LzErrorProgress,
    LzErrorMagic,
} LZ_STATUS, *PLZ_STATUS;

typedef enum _LZ_FLUSH_OPTION {
    LzNoFlush,
    LzFlushNow,
} LZ_FLUSH_OPTION, *PLZ_FLUSH_OPTION;

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

    Read - Stores an optional pointer to a function used to read input data. If
        this is not supplied, then the read buffer must be supplied.

    Write - Stores a pointer to a function used to write output data.

    ReadContext - Stores an unused context pointer available for use by the
        surrounding application. This often stores the input file information.

    WriteContext - Stores an unused context pointer available for use by the
        surrounding application. This often sotres the output file information.

    Input - Stores an optional pointer to an input memory buffer to read
        data from. If the read function is supplied then this buffer is ignored.
        This pointer will be updated as input data is consumed.

    InputSize - Stores the size of available input. This will be updated as
        input data is consumed.

    Output - Stores an optional pointer to the output memory buffer to write
        data to. If the write function is supplied then this buffer is ignored.
        This pointer will be updated as output is written.

    OutputSize - Stores the available size of the output buffer in bytes. This
        will be updated (decreased) as output is written.

    CompressedCrc32 - Stores the CRC32 of the compressed data, which covers
        the properties and the range encoded bytes, but not the length or
        or CRCs.

    UncompressedCrc32 - Stores the CRC32 of the uncompressed data.

    UncompressedSize - Stores the size in bytes of the uncompressed data.

    InternalState - Stores the internal state, opaque outside the library.

--*/

struct _LZ_CONTEXT {
    PVOID Context;
    PLZ_REALLOCATE Reallocate;
    PLZ_REPORT_PROGRESS ReportProgress;
    PLZ_PERFORM_IO Read;
    PLZ_PERFORM_IO Write;
    PVOID ReadContext;
    PVOID WriteContext;
    PVOID Input;
    UINTN InputSize;
    PVOID Output;
    UINTN OutputSize;
    ULONG CompressedCrc32;
    ULONG UncompressedCrc32;
    ULONGLONG UncompressedSize;
    PVOID InternalState;
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
LzLzmaInitializeProperties (
    PLZMA_ENCODER_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine initializes LZMA properties to their defaults.

Arguments:

    Properties - Supplies a pointer to the properties to initialize.

Return Value:

    None.

--*/

LZ_STATUS
LzLzmaInitializeEncoder (
    PLZ_CONTEXT Context,
    PLZMA_ENCODER_PROPERTIES Properties
    );

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

Return Value:

    LZ Status code.

--*/

LZ_STATUS
LzLzmaWriteHeader (
    PLZ_CONTEXT Context
    );

/*++

Routine Description:

    This routine writes the LZMA file header, including a magic value and
    the encoding properties.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

Return Value:

    LZ Status code.

--*/

LZ_STATUS
LzLzmaEncode (
    PLZ_CONTEXT Context
    );

/*++

Routine Description:

    This routine encodes the given initialized LZMA stream.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

Return Value:

    LZ Status code.

--*/

LZ_STATUS
LzLzmaFinishEncode (
    PLZ_CONTEXT Context,
    BOOL WriteCheckFields
    );

/*++

Routine Description:

    This routine flushes the LZMA encoder, and potentially writes the ending
    CRC and length fields.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

    WriteCheckFields - Supplies a boolean indicating whether or not to write
        the check fields.

Return Value:

    LZ Status code.

--*/

LZ_STATUS
LzLzmaInitializeDecoder (
    PLZ_CONTEXT Context,
    PLZMA_ENCODER_PROPERTIES Properties
    );

/*++

Routine Description:

    This routine initializes an LZMA context for decoding.

Arguments:

    Context - Supplies a pointer to the system context, which should be filled
        out by the caller.

    Properties - Supplies an optional pointer to the properties used in the
        upcoming encoding stream. If this is NULL, default properties
        equivalent to an encoding level of five will be set.

Return Value:

    Returns an LZ status code indicating overall success or failure.

--*/

LZ_STATUS
LzLzmaReadHeader (
    PLZ_CONTEXT Context
    );

/*++

Routine Description:

    This routine reads the file header out of a compressed LZMA stream, which
    validates the magic value and reads the properties into the decoder.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    LZ Status code.

--*/

LZ_STATUS
LzLzmaDecode (
    PLZ_CONTEXT Context
    );

/*++

Routine Description:

    This routine decompresses an LZMA stream.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    LZ Status code.

--*/

LZ_STATUS
LzLzmaFinishDecode (
    PLZ_CONTEXT Context,
    BOOL ReadCheckFields
    );

/*++

Routine Description:

    This routine flushes the LZMA decoder, and potentially writes the reads
    and verefies the CRC and length fields.

Arguments:

    Context - Supplies a pointer to the context, which should already be
        initialized by the user.

    ReadCheckFields - Supplies a boolean indicating whether or not to read
        and verify the check fields.

Return Value:

    LZ Status code.

--*/

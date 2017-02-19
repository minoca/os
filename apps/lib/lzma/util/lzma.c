/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzma.c

Abstract:

    This module implements support for the lzma utility.

Author:

    Evan Green 13-Jan-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <minoca/lib/types.h>
#include <minoca/lib/lzma.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_USAGE \
    "Usage: lzma [options] [files...]\n" \
    "Compress or decompress an lzma archive. Options are:\n" \
    "  -c, --compress - Compress data.\n" \
    "  -d, --decompress - Decompress data.\n" \
    "  -i, --input=<file> - Read input from the given file (default stdin).\n" \
    "  -o, --output=<file> - Write output fo the given file (default stoud).\n"\
    "  -0123456789, --level=<level> - Set decompression level (default 5).\n" \
    "  --mode=[0|1] - Set compression mode (default 1: max).\n" \
    "  --dict-size=<size> - Set dictionary size [12, 30] (default 24).\n" \
    "  --fast-bytes=<size> - Set fast byte count [5, 273] (default 128).\n"\
    "  --match-count=<count> - Set match finder cycles.\n" \
    "  --lc=<count> - Set number of literal context bits [0, 8] (default 3).\n"\
    "  --lp=<count> - Set number of literal position bits [0, 4] " \
    "(default 0).\n" \
    "  --pb=<count> - Set number of position bits [0, 4] (default 2).\n" \
    "  --mf=<type> - Set match finder [hc4, bt2, bt3, bt4] (default bt4).\n" \
    "  --eos - Write end of stream marker.\n" \
    "  --help - Display this help message.\n" \
    "  --version -- Display the version information and exit.\n"

#define LZMA_OPTIONS_STRING "cdi:o:0123456789hvV"

#define LZMA_UTIL_VERSION_MAJOR 1
#define LZMA_UTIL_VERSION_MINOR 0

#define LZMA_UTIL_OPTION_VERBOSE 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _LZMA_UTIL_ARGUMENT {
    LzmaUtilInvalid = 256,
    LzmaUtilLevel,
    LzmaUtilMode,
    LzmaUtilDictSize,
    LzmaUtilFastBytes,
    LzmaUtilMatchCount,
    LzmaUtilLc,
    LzmaUtilLp,
    LzmaUtilPb,
    LzmaUtilMf,
    LzmaUtilEos
} LZMA_UTIL_ARGUMENT, *PLZMA_UTIL_ARGUMENT;

typedef enum _LZMA_UTIL_ACTION {
    LzmaActionUnspecified,
    LzmaActionCompress,
    LzmaActionDecompress
} LZMA_UTIL_ACTION, *PLZMA_UTIL_ACTION;

typedef struct _LZMA_UTIL {
    LZ_CONTEXT Lz;
    LZMA_ENCODER_PROPERTIES EncoderProperties;
    ULONG Options;
} LZMA_UTIL, *PLZMA_UTIL;

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
LzpUtilReallocate (
    PVOID Allocation,
    UINTN NewSize
    );

LZ_STATUS
LzpUtilReportProgress (
    PLZ_CONTEXT Context,
    ULONGLONG InputSize,
    ULONGLONG OutputSize
    );

INTN
LzpUtilRead (
    PLZ_CONTEXT Context,
    PVOID Buffer,
    UINTN Size
    );

INTN
LzpUtilWrite (
    PLZ_CONTEXT Context,
    PVOID Buffer,
    UINTN Size
    );

INT
LzpUtilGetNumericOption (
    PCSTR String,
    INT Min,
    INT Max
    );

//
// -------------------------------------------------------------------- Globals
//

struct option LzmaLongOptions[] = {
    {"compress", no_argument, 0, 'c'},
    {"decompress", no_argument, 0, 'd'},
    {"input", required_argument, 0, 'i'},
    {"output", required_argument, 0, 'o'},
    {"level", required_argument, 0, LzmaUtilLevel},
    {"mode", required_argument, 0, LzmaUtilMode},
    {"dict-size", required_argument, 0, LzmaUtilDictSize},
    {"fast-bytes", required_argument, 0, LzmaUtilFastBytes},
    {"match-count", required_argument, 0, LzmaUtilMatchCount},
    {"lc", required_argument, 0, LzmaUtilLc},
    {"lp", required_argument, 0, LzmaUtilLp},
    {"pb", required_argument, 0, LzmaUtilPb},
    {"mf", required_argument, 0, LzmaUtilMf},
    {"eos", no_argument, 0, LzmaUtilEos},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the lzma utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments.

    Arguments - Supplies the array of command line argument strings.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    LZMA_UTIL_ACTION Action;
    LZMA_UTIL Context;
    PSTR InputPath;
    INT Integer;
    LZ_STATUS LzStatus;
    INT Option;
    PSTR OutputPath;
    INT Status;

    Action = LzmaActionUnspecified;
    InputPath = NULL;
    OutputPath = NULL;
    memset(&Context, 0, sizeof(LZMA_UTIL));
    Context.Lz.Context = &Context;
    Context.Lz.Reallocate = LzpUtilReallocate;
    Context.Lz.ReportProgress = LzpUtilReportProgress;
    Context.Lz.Read = LzpUtilRead;
    Context.Lz.Write = LzpUtilWrite;
    LzLzmaEncoderInitializeProperties(&(Context.EncoderProperties));
    Context.EncoderProperties.WriteEndMark = TRUE;
    Status = 1;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             LZMA_OPTIONS_STRING,
                             LzmaLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            goto MainEnd;
        }

        switch (Option) {
        case 'c':
        case 'd':
            if (Action != LzmaActionUnspecified) {
                fprintf(stderr, "Error: Cannot specify multiple actions.\n");
                goto MainEnd;
            }

            Action = LzmaActionCompress;
            if (Option == 'd') {
                Action = LzmaActionDecompress;
            }

            break;

        case 'i':
            InputPath = optarg;
            break;

        case 'o':
            OutputPath = optarg;
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            Context.EncoderProperties.Level = Option - '0';
            break;

        case LzmaUtilLevel:
            Integer = LzpUtilGetNumericOption(optarg, 1, 9);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.Level = Integer;
            break;

        case LzmaUtilMode:
            Integer = LzpUtilGetNumericOption(optarg, 0, 1);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.Algorithm = Integer;
            break;

        case LzmaUtilDictSize:
            Integer = LzpUtilGetNumericOption(optarg, 12, 30);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.DictionarySize = Integer;
            break;

        case LzmaUtilFastBytes:
            Integer = LzpUtilGetNumericOption(optarg, 5, 273);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.FastBytes = Integer;
            break;

        case LzmaUtilMatchCount:
            Integer = LzpUtilGetNumericOption(optarg, 1, 2 << 30);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.MatchCount = Integer;
            break;

        case LzmaUtilLc:
            Integer = LzpUtilGetNumericOption(optarg, 0, 8);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.Lc = Integer;
            break;

        case LzmaUtilLp:
            Integer = LzpUtilGetNumericOption(optarg, 0, 4);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.Lp = Integer;
            break;

        case LzmaUtilPb:
            Integer = LzpUtilGetNumericOption(optarg, 0, 4);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.EncoderProperties.Pb = Integer;
            break;

        case LzmaUtilMf:
            Context.EncoderProperties.BinTreeMode = TRUE;
            if (strcmp(optarg, "hc4") == 0) {
                Context.EncoderProperties.BinTreeMode = FALSE;
                Context.EncoderProperties.HashByteCount = 4;

            } else if (strcmp(optarg, "bt2") == 0) {
                Context.EncoderProperties.HashByteCount = 2;

            } else if (strcmp(optarg, "bt3") == 0) {
                Context.EncoderProperties.HashByteCount = 3;

            } else if (strcmp(optarg, "bt4") == 0) {
                Context.EncoderProperties.HashByteCount = 4;

            } else {
                fprintf(stderr,
                        "Error: Invalid match finder mode: %s\n",
                        optarg);

                goto MainEnd;
            }

            break;

        case LzmaUtilEos:
            Context.EncoderProperties.WriteEndMark = TRUE;
            break;

        case 'v':
            Context.Options |= LZMA_UTIL_OPTION_VERBOSE;
            break;

        case 'V':
            printf("Lzma utility version %d.%d.\n",
                   LZMA_UTIL_VERSION_MAJOR,
                   LZMA_UTIL_VERSION_MINOR);

            return 1;

        case 'h':
            printf(LZMA_USAGE);
            return 1;

        default:

            assert(FALSE);

            goto MainEnd;
        }
    }

    if (Action == LzmaActionUnspecified) {
        fprintf(stderr,
                "Error: Specify either -c or -d. Try --help for usage\n");

        goto MainEnd;
    }

    //
    // Open up the input and output files if specified.
    //

    if (InputPath != NULL) {
        if (strcmp(InputPath, "-") == 0) {
            Context.Lz.ReadContext = stdin;

        } else {
            Context.Lz.ReadContext = fopen(InputPath, "rb");
            if (Context.Lz.ReadContext == NULL) {
                fprintf(stderr,
                        "Error: Failed to open %s: %s.\n",
                        InputPath,
                        strerror(errno));

                goto MainEnd;
            }
        }

    } else {
        if (isatty(STDIN_FILENO)) {
            fprintf(stderr,
                    "Error: Not reading from interactive terminal. Use "
                    "--input=- to force this behavior.\n");

            goto MainEnd;
        }

        Context.Lz.ReadContext = stdin;
    }

    if (OutputPath != NULL) {
        if (strcmp(OutputPath, "-") == 0) {
            Context.Lz.WriteContext = stdout;

        } else {
            Context.Lz.WriteContext = fopen(OutputPath, "wb");
            if (Context.Lz.WriteContext == NULL) {
                fprintf(stderr,
                        "Error: Failed to open %s: %s.\n",
                        OutputPath,
                        strerror(errno));

                goto MainEnd;
            }
        }

    } else {
        if (isatty(STDOUT_FILENO)) {
            fprintf(stderr,
                    "Error: Not writing to interactive terminal. Use "
                    "--output=- to force this behavior.\n");

            goto MainEnd;
        }

        Context.Lz.WriteContext = stdout;
    }

    if (Action == LzmaActionCompress) {
        LzStatus = LzLzmaEncodeStream(&(Context.EncoderProperties),
                                      &(Context.Lz));

        if (LzStatus != LzSuccess) {
            fprintf(stderr,
                    "Error: Failed to encode: %d.\n",
                    LzStatus);

            goto MainEnd;
        }

    } else {
        fprintf(stderr, "TODO: Not yet implemented");
        goto MainEnd;
    }

    Status = 0;

MainEnd:
    if ((Context.Lz.ReadContext != NULL) &&
        (Context.Lz.ReadContext != stdin)) {

        fclose(Context.Lz.ReadContext);
    }

    if ((Context.Lz.WriteContext != NULL) &&
        (Context.Lz.WriteContext != stdout)) {

        fclose(Context.Lz.WriteContext);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
LzpUtilReallocate (
    PVOID Allocation,
    UINTN NewSize
    )

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

{

    return realloc(Allocation, NewSize);
}

LZ_STATUS
LzpUtilReportProgress (
    PLZ_CONTEXT Context,
    ULONGLONG InputSize,
    ULONGLONG OutputSize
    )

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

{

    PLZMA_UTIL Util;

    Util = Context->Context;
    if ((Util->Options & LZMA_UTIL_OPTION_VERBOSE) != 0) {
        printf("%lld/%lld", InputSize, OutputSize);
    }

    return LzSuccess;
}

INTN
LzpUtilRead (
    PLZ_CONTEXT Context,
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine performs a read from the given input file.

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

    FILE *File;
    UINTN Result;

    File = Context->ReadContext;
    Result = fread(Buffer, 1, Size, File);
    if (Result == 0) {
        if (ferror(File)) {
            return -1;
        }
    }

    return Result;
}

INTN
LzpUtilWrite (
    PLZ_CONTEXT Context,
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine performs a write to the output stream.

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

    FILE *File;
    UINTN Result;

    File = Context->WriteContext;
    Result = fwrite(Buffer, 1, Size, File);
    if (Result == 0) {
        if (ferror(File)) {
            return -1;
        }
    }

    return Result;
}

INT
LzpUtilGetNumericOption (
    PCSTR String,
    INT Min,
    INT Max
    )

/*++

Routine Description:

    This routine converts an option string into an integer, and complains if it
    is not within range.

Arguments:

    String - Supplies the string to convert.

    Min - Supplies the minimum valid value, inclusive.

    Max - Supplies the maximum valid value, inclusive.

Return Value:

    Returns the integer value on success.

    -1 on failure.

--*/

{

    PSTR AfterScan;
    INT Result;

    Result = strtol(String, &AfterScan, 0);
    if ((AfterScan == String) || (*AfterScan != '\0')) {
        fprintf(stderr, "Error: Invalid integer: %s\n", String);
        return -1;
    }

    if ((Result < Min) || (Result > Max)) {
        fprintf(stderr,
                "Error: Value %d is not within the required range of "
                "%d - %d.\n",
                Result,
                Min,
                Max);

        return -1;
    }

    return Result;
}


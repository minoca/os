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
#include <fcntl.h>
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

#if defined(O_BINARY) && (O_BINARY != O_TEXT)

#include <io.h>

#define SET_BINARY_MODE(_Descriptor) _setmode(_Descriptor, O_BINARY)

#else

#define SET_BINARY_MODE(_Descriptor)

#endif

//
// ---------------------------------------------------------------- Definitions
//

#define LZMA_USAGE \
    "Usage: lzma [options] [files...]\n" \
    "Compress or decompress an lzma archive. Options are:\n" \
    "  -c, --compress - Compress data.\n" \
    "  -d, --decompress - Decompress data.\n" \
    "  -i, --input=<file> - Read input from the given file (default stdin).\n" \
    "  -l, --list - Show the file name, uncompressed size, compressed size\n" \
    "      and compression ratio. If combined with -v, also prints \n" \
    "      uncompressed and compressed CRC32.\n" \
    "  -o, --output=<file> - Write output fo the given file (default stoud).\n"\
    "  -0123456789, --level=<level> - Set compression level (default 5).\n" \
    "  --mode=[0|1] - Set compression mode (default 1: max).\n" \
    "  --dict-size=<size> - Set dictionary size [12, 30] (default 24).\n" \
    "  --fast-bytes=<size> - Set fast byte count [5, 273] (default 128).\n"\
    "  --match-count=<count> - Set match finder cycles.\n" \
    "  --memory-test=<count> - Run in memory buffer mode, with a specified \n" \
    "      buffer size.\n" \
    "  --lc=<count> - Set number of literal context bits [0, 8] (default 3).\n"\
    "  --lp=<count> - Set number of literal position bits [0, 4] " \
    "(default 0).\n" \
    "  --pb=<count> - Set number of position bits [0, 4] (default 2).\n" \
    "  --mf=<type> - Set match finder [hc4, bt2, bt3, bt4] (default bt4).\n" \
    "  --no-eos - Do not write end of stream marker.\n" \
    "  --help - Display this help message.\n" \
    "  --version -- Display the version information and exit.\n"

#define LZMA_OPTIONS_STRING "cdi:lo:0123456789hvV"

#define LZMA_UTIL_VERSION_MAJOR 1
#define LZMA_UTIL_VERSION_MINOR 0

#define LZMA_UTIL_OPTION_VERBOSE 0x00000001
#define LZMA_UTIL_OPTION_LIST 0x00000002

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
    LzmaUtilMemoryTest,
    LzmaUtilLc,
    LzmaUtilLp,
    LzmaUtilPb,
    LzmaUtilMf,
    LzmaUtilNoEos
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
    UINTN MemoryTest;
} LZMA_UTIL, *PLZMA_UTIL;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
LzpUtilProcessStream (
    PLZMA_UTIL Context,
    PCSTR InputPath,
    PCSTR OutputPath,
    LZMA_UTIL_ACTION Action
    );

INT
LzpUtilRunMemoryTest (
    PLZMA_UTIL Context,
    LZMA_UTIL_ACTION Action
    );

PVOID
LzpUtilReallocate (
    PVOID Allocation,
    UINTN NewSize
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

PCSTR
LzpUtilGetErrorString (
    LZ_STATUS Status
    );

//
// -------------------------------------------------------------------- Globals
//

struct option LzmaLongOptions[] = {
    {"compress", no_argument, 0, 'c'},
    {"decompress", no_argument, 0, 'd'},
    {"input", required_argument, 0, 'i'},
    {"list", no_argument, 0, 'l'},
    {"output", required_argument, 0, 'o'},
    {"level", required_argument, 0, LzmaUtilLevel},
    {"mode", required_argument, 0, LzmaUtilMode},
    {"dict-size", required_argument, 0, LzmaUtilDictSize},
    {"fast-bytes", required_argument, 0, LzmaUtilFastBytes},
    {"match-count", required_argument, 0, LzmaUtilMatchCount},
    {"memory-test", required_argument, 0, LzmaUtilMemoryTest},
    {"lc", required_argument, 0, LzmaUtilLc},
    {"lp", required_argument, 0, LzmaUtilLp},
    {"pb", required_argument, 0, LzmaUtilPb},
    {"mf", required_argument, 0, LzmaUtilMf},
    {"no-eos", no_argument, 0, LzmaUtilNoEos},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

PCSTR LzStatusStrings[LzErrorCount] = {
    "Success",
    "Stream complete",
    "Corrupt data",
    "Allocation failure",
    "CRC error",
    "Unsupported",
    "Invalid parameter",
    "Unexpected end of input",
    "Unexpected end of output",
    "Read error",
    "Write error",
    "Progress error",
    "Invalid magic value"
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
    INT ArgumentIndex;
    LZMA_UTIL Context;
    PSTR InputPath;
    INT Integer;
    INT Option;
    PSTR OutputPath;
    INT Status;
    INT TotalStatus;

    Action = LzmaActionUnspecified;
    InputPath = NULL;
    OutputPath = NULL;
    memset(&Context, 0, sizeof(LZMA_UTIL));
    Context.Lz.Context = &Context;
    Context.Lz.Reallocate = LzpUtilReallocate;
    Context.Lz.Read = LzpUtilRead;
    Context.Lz.Write = LzpUtilWrite;
    LzLzmaInitializeProperties(&(Context.EncoderProperties));
    Context.EncoderProperties.EndMark = TRUE;
    Status = 1;
    TotalStatus = 0;

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

        case 'l':
            Context.Options |= LZMA_UTIL_OPTION_LIST;
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

        case LzmaUtilMemoryTest:
            Integer = LzpUtilGetNumericOption(optarg, 1, 1 << 30);
            if (Integer < 0) {
                goto MainEnd;
            }

            Context.MemoryTest = Integer;
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

        case LzmaUtilNoEos:
            Context.EncoderProperties.EndMark = FALSE;
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
    // Print the listing header.
    //

    if ((Context.Options & LZMA_UTIL_OPTION_LIST) != 0) {
        if ((Context.Options & LZMA_UTIL_OPTION_VERBOSE) != 0) {
            fprintf(stderr,
                    "%-15s%-15s%-7s%-10s%-10s%s\n",
                    "Uncompressed",
                    "Compressed",
                    "Ratio",
                    "UncompCRC",
                    "ComprCRC",
                    "Name");

        } else {
            fprintf(stderr,
                    "%-15s%-15s%-7s%s\n",
                    "Uncompressed",
                    "Compressed",
                    "Ratio",
                    "Name");
        }
    }

    if (optind < ArgumentCount) {
        if ((InputPath != NULL) || (OutputPath != NULL)) {
            fprintf(stderr,
                    "lzma: Cannot mix -i/-o and command line arguments.\n");

            Status = EINVAL;
            goto MainEnd;
        }

        //
        // Process each argument on the command line.
        //

        for (ArgumentIndex = optind;
             ArgumentIndex < ArgumentCount;
             ArgumentIndex += 1) {

            Status = LzpUtilProcessStream(&Context,
                                          Arguments[ArgumentIndex],
                                          NULL,
                                          Action);

            if (Status != 0) {
                TotalStatus = Status;
            }
        }

    } else {
        if (InputPath == NULL) {
            if (isatty(STDIN_FILENO)) {
                fprintf(stderr,
                        "Error: Not reading from interactive terminal. Use "
                        "--input=- to force this behavior.\n");

                goto MainEnd;
            }

            InputPath = "-";
        }

        if (OutputPath == NULL) {
            if (isatty(STDOUT_FILENO)) {
                fprintf(stderr,
                        "Error: Not writing to interactive terminal. Use "
                        "--output=- to force this behavior.\n");

                goto MainEnd;
            }

            OutputPath = "-";
        }

        Status = LzpUtilProcessStream(&Context, InputPath, OutputPath, Action);
    }

MainEnd:
    if ((Status == 0) && (TotalStatus != 0)) {
        Status = TotalStatus;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
LzpUtilProcessStream (
    PLZMA_UTIL Context,
    PCSTR InputPath,
    PCSTR OutputPath,
    LZMA_UTIL_ACTION Action
    )

/*++

Routine Description:

    This routine performs a compress or decompress operation on a single stream.

Arguments:

    Context - Supplies a pointer to the application context.

    InputPath - Supplies the input path of the stream to open, or "-" to use
        stdin.

    OutputPath - Supplies the path to the output stream, or "-" to use stdout.

    Action - Supplies the action to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PCSTR BaseName;
    size_t InLength;
    PSTR LastDot;
    PLZ_CONTEXT Lz;
    LZ_STATUS LzStatus;
    PSTR OutPathBuffer;
    size_t OutPathSize;
    ULONG Ratio;
    CHAR RatioString[6];
    PCSTR Search;
    INT Status;

    Lz = &(Context->Lz);
    OutPathBuffer = NULL;
    Status = 2;

    //
    // Open up the input and output files if specified.
    //

    if (strcmp(InputPath, "-") == 0) {
        Lz->ReadContext = stdin;
        SET_BINARY_MODE(fileno(stdin));

    } else {
        Lz->ReadContext = fopen(InputPath, "rb");
        if (Lz->ReadContext == NULL) {
            fprintf(stderr,
                    "Error: Failed to open %s: %s.\n",
                    InputPath,
                    strerror(errno));

            goto ProcessStreamEnd;
        }
    }

    //
    // Come up with an output name if there is none.
    //

    if (OutputPath == NULL) {
        InLength = strlen(InputPath);
        if (Action == LzmaActionCompress) {
            OutPathSize = InLength + 4;
            OutPathBuffer = malloc(OutPathSize);
            if (OutPathBuffer == NULL) {
                goto ProcessStreamEnd;
            }

            snprintf(OutPathBuffer, OutPathSize, "%s.lz", InputPath);

        //
        // Try to strip off the last extension.
        //

        } else {
            OutPathBuffer = strdup(InputPath);
            if (OutPathBuffer == NULL) {
                goto ProcessStreamEnd;
            }

            //
            // Find the basename of the string.
            //

            Search = OutPathBuffer;
            BaseName = Search;
            while (*Search != '\0') {
                if ((*Search == '/') || (*Search == '\\')) {
                    BaseName = Search + 1;
                }

                Search += 1;
            }

            //
            // Find the last dot, which is the extension. If there is no
            // extension, calle it <file>.out.
            //

            LastDot = strrchr(BaseName, '.');
            if ((LastDot == NULL) || (LastDot == BaseName)) {
                free(OutPathBuffer);
                OutPathSize = InLength + 5;
                OutPathBuffer = malloc(OutPathSize);
                if (OutPathBuffer == NULL) {
                    goto ProcessStreamEnd;
                }

                snprintf(OutPathBuffer, OutPathSize, "%s.out", InputPath);

            } else {
                *LastDot = '\0';
            }

        }

        OutputPath = OutPathBuffer;
    }

    //
    // Open up the output.
    //

    if (strcmp(OutputPath, "-") == 0) {
        Lz->WriteContext = stdout;
        SET_BINARY_MODE(fileno(stdout));

    } else {
        Lz->WriteContext = fopen(OutputPath, "wb");
        if (Lz->WriteContext == NULL) {
            fprintf(stderr,
                    "Error: Failed to open %s: %s.\n",
                    OutputPath,
                    strerror(errno));

            goto ProcessStreamEnd;
        }
    }

    //
    // If memory test mode was requested, go off and do things the buffer way.
    //

    if (Context->MemoryTest != 0) {
        Status = LzpUtilRunMemoryTest(Context, Action);
        goto ProcessStreamEnd;
    }

    if (Action == LzmaActionCompress) {
        LzStatus = LzLzmaInitializeEncoder(Lz,
                                           &(Context->EncoderProperties),
                                           TRUE);

        if (LzStatus != LzSuccess) {
            fprintf(stderr,
                    "Error: Failed to initialize encoder: %s.\n",
                    LzpUtilGetErrorString(LzStatus));

            goto ProcessStreamEnd;
        }

        LzStatus = LzLzmaEncode(Lz, LzFlushNow);
        if (LzStatus != LzStreamComplete) {
            fprintf(stderr,
                    "Error: Failed to encode: %s.\n",
                    LzpUtilGetErrorString(LzStatus));

            goto ProcessStreamEnd;
        }

        LzStatus = LzLzmaFinishEncode(Lz);
        if (LzStatus != LzStreamComplete) {
            fprintf(stderr,
                    "Error: Failed to finish: %s.\n",
                    LzpUtilGetErrorString(LzStatus));

            goto ProcessStreamEnd;
        }

    } else {
        LzStatus = LzLzmaInitializeDecoder(Lz, NULL, TRUE);
        if (LzStatus != LzSuccess) {
            fprintf(stderr,
                    "Error: Failed to initialize decoder: %s.\n",
                    LzpUtilGetErrorString(LzStatus));

            goto ProcessStreamEnd;
        }

        LzStatus = LzLzmaDecode(Lz, LzFlushNow);
        if (LzStatus != LzStreamComplete) {
            fprintf(stderr,
                    "Error: Failed to decode: %s.\n",
                    LzpUtilGetErrorString(LzStatus));

            goto ProcessStreamEnd;
        }

        LzLzmaFinishDecode(Lz);
    }

    //
    // Spit out the listing if requested.
    //

    if ((Context->Options & LZMA_UTIL_OPTION_LIST) != 0) {
        Ratio = (Lz->CompressedSize * 1000ULL) / Lz->UncompressedSize;
        snprintf(RatioString,
                 sizeof(RatioString),
                 "%d.%d%%",
                 Ratio / 10,
                 Ratio % 10);

        if ((Context->Options & LZMA_UTIL_OPTION_VERBOSE) != 0) {
            fprintf(stderr,
                    "%-15lld%-15lld%-7s%08x  %08x  %s\n",
                    Lz->UncompressedSize,
                    Lz->CompressedSize,
                    RatioString,
                    Lz->UncompressedCrc32,
                    Lz->CompressedCrc32,
                    InputPath);

        } else {
            fprintf(stderr,
                    "%-15lld%-15lld%-7s%s\n",
                    Lz->UncompressedSize,
                    Lz->CompressedSize,
                    RatioString,
                    InputPath);
        }
    }

    Status = 0;

ProcessStreamEnd:
    if ((Lz->ReadContext != NULL) && (Lz->ReadContext != stdin)) {
        fclose(Lz->ReadContext);
        Lz->ReadContext = NULL;
    }

    if ((Lz->WriteContext != NULL) && (Lz->WriteContext != stdout)) {
        fclose(Lz->WriteContext);
        Lz->WriteContext = NULL;
    }

    return Status;
}

INT
LzpUtilRunMemoryTest (
    PLZMA_UTIL Context,
    LZMA_UTIL_ACTION Action
    )

/*++

Routine Description:

    This routine performs a compress or decompress operation in memory buffer
    mode. It is useful for testing the encoder/decoder.

Arguments:

    Context - Supplies a pointer to the application context.

    Action - Supplies the action to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINTN BufferSize;
    LZ_FLUSH_OPTION Flush;
    PVOID InBuffer;
    PLZ_CONTEXT Lz;
    LZ_STATUS LzStatus;
    PVOID OutBuffer;
    INTN Size;
    INT Status;
    PSTR Verb;

    Lz = &(Context->Lz);
    BufferSize = Context->MemoryTest;
    InBuffer = malloc(BufferSize * 2);
    if (InBuffer == NULL) {
        Status = ENOMEM;
        goto RunMemoryTestEnd;
    }

    Flush = LzNoFlush;
    OutBuffer = InBuffer + BufferSize;
    Context->Lz.Output = OutBuffer;
    Context->Lz.OutputSize = BufferSize;
    Context->Lz.Read = NULL;
    Context->Lz.Write = NULL;
    if (Action == LzmaActionCompress) {
        Verb = "encode";
        LzStatus = LzLzmaInitializeEncoder(Lz,
                                           &(Context->EncoderProperties),
                                           TRUE);

    } else {
        Verb = "decode";
        LzStatus = LzLzmaInitializeDecoder(Lz, NULL, TRUE);
    }

    if (LzStatus != LzSuccess) {
        fprintf(stderr,
                "Error: Failed to initialize %sr: %s.\n",
                Verb,
                LzpUtilGetErrorString(LzStatus));

        Status = 1;
        goto RunMemoryTestEnd;
    }

    //
    // Loop crunching data.
    //

    while (TRUE) {

        //
        // Send out all pending write data.
        //

        if (Lz->OutputSize < BufferSize) {
            Size = BufferSize - Lz->OutputSize;
            if (LzpUtilWrite(Lz, OutBuffer, Size) != Size) {
                Status = errno;
                fprintf(stderr, "lzma: Write Error: %s\n", strerror(Status));
                goto RunMemoryTestEnd;
            }

            Lz->Output = OutBuffer;
            Lz->OutputSize = BufferSize;
        }

        //
        // Read in any input needed to make the buffer full again.
        //

        if ((Lz->InputSize < BufferSize) && (Flush == LzNoFlush)) {
            if (Lz->InputSize > 0) {
                memmove(InBuffer, Lz->Input, Lz->InputSize);
            }

            Size = LzpUtilRead(Lz,
                               InBuffer + Lz->InputSize,
                               BufferSize - Lz->InputSize);

            if (Size <= 0) {
                if (Size == 0) {
                    Flush = LzInputFinished;

                } else {
                    Status = errno;
                    fprintf(stderr, "lzma: Read Error: %s\n", strerror(Status));
                    goto RunMemoryTestEnd;
                }
            }

            Lz->Input = InBuffer;
            Lz->InputSize += Size;
        }

        if (Action == LzmaActionCompress) {
            LzStatus = LzLzmaEncode(Lz, Flush);

        } else {
            LzStatus = LzLzmaDecode(Lz, Flush);
        }

        if (LzStatus == LzStreamComplete) {

            //
            // Do any final write.
            //

            if (Lz->OutputSize < BufferSize) {
                Size = BufferSize - Lz->OutputSize;
                if (LzpUtilWrite(Lz, OutBuffer, Size) != Size) {
                    Status = errno;
                    fprintf(stderr,
                            "lzma: Write Error: %s\n",
                            strerror(Status));

                    goto RunMemoryTestEnd;
                }
            }

            break;

        } else if (LzStatus != LzSuccess) {
            fprintf(stderr,
                    "Error: Failed to %s: %s.\n",
                    Verb,
                    LzpUtilGetErrorString(LzStatus));

            Status = 1;
            goto RunMemoryTestEnd;
        }

        if ((Lz->InputSize == BufferSize) && (Lz->OutputSize == BufferSize)) {
            fprintf(stderr, "Error: No progress was made!\n");
            Status = 1;
            goto RunMemoryTestEnd;
        }
    }

    if (Action == LzmaActionCompress) {
        LzStatus = LzLzmaFinishEncode(Lz);
        if (LzStatus != LzStreamComplete) {
            fprintf(stderr,
                    "Error: Failed to finish encoder: %s.\n",
                    LzpUtilGetErrorString(LzStatus));

            Status = 1;
            goto RunMemoryTestEnd;
        }

    } else {
        LzLzmaFinishDecode(Lz);
    }

    Status = 0;

RunMemoryTestEnd:
    if (InBuffer != NULL) {
        free(InBuffer);
    }

    return Status;
}

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

PCSTR
LzpUtilGetErrorString (
    LZ_STATUS Status
    )

/*++

Routine Description:

    This routine returns an error string associated with the given status
    code.

Arguments:

    Status - Supplies the LZ status code to convert.

Return Value:

    Returns a string representation of the given error.

--*/

{

    if ((Status < 0) ||
        (Status > (sizeof(LzStatusStrings) / sizeof(LzStatusStrings[0])))) {

        return "Unknown error";
    }

    return LzStatusStrings[Status];
}


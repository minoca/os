/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dd.c

Abstract:

    This module implements the dd utility.

Author:

    Evan Green 13-May-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DD_VERSION_MAJOR 1
#define DD_VERSION_MINOR 0

#define DD_USAGE                                                               \
    "usage: dd [operands]\n"                                                   \
    "The head command copies the contents of one file to another, \n"          \
    "potentially block by block, and potentially with conversions.\n"          \
    "Specifications are:\n"                                                    \
    "  bs=bytes -- Read and write the given number of bytes at a time.\n"      \
    "  cbs=bytes -- Convert the given number of bytes at a time.\n"            \
    "  conv=list -- Convert the file according to the given \n"                \
    "    comma-separated list.\n"                                              \
    "  count=N -- Copy only the given number of input blocks.\n"               \
    "  ibs=bytes -- Read the given number of bytes at a time \n"               \
    "    (512 by default).\n"                                                  \
    "  if=file -- Use the given file path as an input rather than stdin.\n"    \
    "  iflag=flags -- Use the given comma-separated flags for the input.\n"    \
    "  obs=bytes -- Write the given number of bytes at at time.\n"             \
    "  of=file -- Write to the given output file instead of stdout.\n"         \
    "  oflag=flags -- Use the given comma-separated flags for the output.\n"   \
    "  seek=N -- Skip N obs-sized blocks at the start of the output.\n"        \
    "  skip=N -- Skip N ibs-sized blocks from the beginning of the input.\n"   \
    "Values for conv (conversion can be):\n"                                   \
    "  block -- Pad newline-terminated records with spaces to cbs-size.\n"     \
    "  unblock -- Replace trailing spaces in cbs-size records with newlines.\n"\
    "  lcase -- Change all characters to lower case.\n"                        \
    "  ucase -- Change all characters to upper case.\n"                        \
    "  sparse -- Try to seek instead of writing the output for NUL input \n"   \
    "    blocks.\n"                                                            \
    "  swab -- Swap every two input bytes.\n"                                  \
    "  sync -- Pad every input block with zeros out to ibs-size. \n"           \
    "    If used with block or unblock, pads with spaces rather than zeros.\n" \
    "  excl -- Fail if the output file already exists.\n"                      \
    "  nocreat -- Do not create the file.\n"                                   \
    "  notrunc -- Do not truncate the file.\n"                                 \
    "  noerror - Continue after read errors.\n"                                \
    "Values for flags:\n"                                                      \
    "  fullblock -- Accumulate full blocks of input.\n"                        \
    "  nonblock -- Use non-blocking I/O.\n"                                    \
    "  noatime -- Do not update access time when opening the file.\n"          \
    "  noctty -- Do not adopt a file as the controlling terminal.\n"           \
    "  nofollow -- Do not follow symlinks.\n"                                  \
    "  count_bytes -- Treat count=N as a byte count (input only).\n"           \
    "  skip_bytes -- Treat skip=N as a byte count (input only).\n"             \
    "  seek_bytes -- Treat seek=N as a byte count (output only).\n"            \
    "Sending a SIGUSR1 to dd causes it to print its current I/O statistics\n"  \
    "and keep going. Sending a SIGINT causes dd to print its current I/O\n"    \
    "statistics and exit.\n"                                                   \
    "Options are:\n"                                                           \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version - Show the application version information and exit.\n"

#define DD_OPTIONS_STRING ""

#define DD_DEFAULT_BLOCK_SIZE 512
#define DD_DEFAULT_IN_OPEN_FLAGS (O_RDONLY)
#define DD_DEFAULT_OUT_OPEN_FLAGS (O_CREAT | O_TRUNC | O_WRONLY)
#define DD_DEFAULT_CREATION_MASK 0644

//
// Define dd options.
//

//
// Set this option to pad newline-terminated records to conversion block size.
//

#define DD_OPTION_BLOCK 0x00000001

//
// Set this option to replace trailing spaces in conversion-block sized
// records with a newline.
//

#define DD_OPTION_UNBLOCK 0x00000002

//
// Set this option to convert characters to lower case.
//

#define DD_OPTION_LOWERCASE 0x00000004

//
// Set this option to convert characters to upper case.
//

#define DD_OPTION_UPPERCASE 0x00000008

//
// Set this option to try and seek rather than output zeroed input blocks.
//

#define DD_OPTION_SPARSE 0x00000010

//
// Set this option to swap every two bytes of input.
//

#define DD_OPTION_SWAB 0x00000020

//
// Set this option to pad every input block with zeros out to input block
// size. Pads with spaces when used with block or unblock.
//

#define DD_OPTION_SYNC 0x00000040

//
// Set this option to continue even on errors.
//

#define DD_OPTION_NO_ERROR 0x00000080

//
// Set this option to treat the input count as bytes.
//

#define DD_OPTION_COUNT_BYTES 0x00000100

//
// Set this option to treat the skip count as bytes.
//

#define DD_OPTION_SKIP_BYTES 0x00000200

//
// Set this option to tread the seek count as bytes.
//

#define DD_OPTION_SEEK_BYTES 0x00000400

//
// Set this option to accumulate full input blocks.
//

#define DD_OPTION_FULL_BLOCKS 0x00000800

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context for a DD application instance.

Members:

    InBlockSize - Stores the input block size.

    OutBlockSize - Stores the output block size.

    ConvertBlockSize - Stores the character conversion size.

    Options - Stores application options. See DD_OPTION_* definitions.

    Count - Stores the number of input bytes to copy.

    OutSkip - Stores the number of bytes to skip on output.

    InSkip - Stores the number of bytes to skip on input.

    InOpenFlags - Stores the input open flags to use.

    OutOpenFlags - Stores the output open flags to use.

    StartTime - Stores the start time for the operation.

    InWhole - Stores the count of whole blocks that have been read in.

    InPartial - Stores the count of partial blocks that have been read in.

    OutWhole - Stores the count of whole blocks that have been written out.

    OutPartial - Stores the count of partial blocks that have been written out.

    BytesComplete - Stores the count of bytes that have been copied.

    Exit - Stores a boolean indicating the application should exit because a
        SIGINT occurred.

    PrintRequest - Stores a boolean indicating that there is a request for
        I/O statistics pending.

--*/

typedef struct _DD_CONTEXT {
    UINTN InBlockSize;
    UINTN OutBlockSize;
    UINTN ConvertBlockSize;
    ULONG Options;
    ULONGLONG Count;
    ULONGLONG OutSkip;
    ULONGLONG InSkip;
    INT InOpenFlags;
    INT OutOpenFlags;
    struct timespec StartTime;
    ULONGLONG InWholeBlocks;
    ULONGLONG InPartialBlocks;
    ULONGLONG OutWholeBlocks;
    ULONGLONG OutPartialBlocks;
    ULONGLONG BytesComplete;
    BOOL Exit;
    BOOL PrintRequest;
} DD_CONTEXT, *PDD_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

void
DdSignalHandler (
    int Signal
    );

VOID
DdPrintIoStatistics (
    PDD_CONTEXT Context
    );

INT
DdParseConversionArguments (
    PDD_CONTEXT Context,
    PSTR Argument
    );

INT
DdParseFileArguments (
    PDD_CONTEXT Context,
    PSTR Argument,
    BOOL IsInput
    );

//
// -------------------------------------------------------------------- Globals
//

struct option DdLongOptions[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// Store the global for the dd context, used by the signal handlers.
//

PDD_CONTEXT DdContext;

//
// ------------------------------------------------------------------ Functions
//

INT
DdMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the head utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    struct sigaction Action;
    PSTR Argument;
    ULONG ArgumentIndex;
    PUCHAR Buffer;
    UINTN BufferSize;
    UINTN ByteIndex;
    ssize_t BytesComplete;
    UINTN BytesThisRound;
    DD_CONTEXT Context;
    ULONGLONG Count;
    PSTR InPath;
    int Input;
    INT Option;
    struct sigaction OriginalSigint;
    struct sigaction OriginalSigusr1;
    PSTR OutPath;
    int Output;
    INT Status;
    UCHAR Swap;
    INT TotalStatus;

    memset(&Context, 0, sizeof(DD_CONTEXT));
    DdContext = &Context;
    Context.InBlockSize = DD_DEFAULT_BLOCK_SIZE;
    Context.OutBlockSize = DD_DEFAULT_BLOCK_SIZE;
    Context.ConvertBlockSize = 1;
    Context.InOpenFlags = DD_DEFAULT_IN_OPEN_FLAGS;
    Context.OutOpenFlags = DD_DEFAULT_OUT_OPEN_FLAGS;
    InPath = NULL;
    Input = -1;
    OutPath = NULL;
    Output = -1;
    TotalStatus = 0;
    Status = 0;

    //
    // Wire up the signal handlers.
    //

    memset(&Action, 0, sizeof(struct sigaction));
    Action.sa_handler = DdSignalHandler;
    sigaction(SIGINT, &Action, &OriginalSigint);
    sigaction(SIGUSR1, &Action, &OriginalSigusr1);

    //
    // Process the control arguments, which is pretty much just --help and
    // --version.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             DD_OPTIONS_STRING,
                             DdLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'V':
            SwPrintVersion(DD_VERSION_MAJOR, DD_VERSION_MINOR);
            return 1;

        case 'h':
            printf(DD_USAGE);
            Status = 1;
            goto MainEnd;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        if (strstr(Argument, "bs=") == Argument) {
            Argument += 3;
            Context.InBlockSize = SwParseFileSize(Argument);
            if (Context.InBlockSize == -1ULL) {
                SwPrintError(0, Argument, "Invalid block size");
                Status = EINVAL;
                goto MainEnd;
            }

            Context.OutBlockSize = Context.InBlockSize;

        } else if (strstr(Argument, "cbs=") == Argument) {
            Argument += 4;
            Context.ConvertBlockSize = SwParseFileSize(Argument);
            if (Context.ConvertBlockSize == -1ULL) {
                SwPrintError(0, Argument, "Invalid block size");
                Status = EINVAL;
                goto MainEnd;
            }

        } else if (strstr(Argument, "conv=") == Argument) {
            Argument += 5;
            Status = DdParseConversionArguments(&Context, Argument);
            if (Status != 0) {
                SwPrintError(Status, Argument, "Invalid conversion argument");
                goto MainEnd;
            }

        } else if (strstr(Argument, "count=") == Argument) {
            Argument += 6;
            Context.Count = SwParseFileSize(Argument);
            if (Context.Count == -1ULL) {
                SwPrintError(0, Argument, "Invalid size");
                Status = EINVAL;
                goto MainEnd;
            }

        } else if (strstr(Argument, "ibs=") == Argument) {
            Argument += 4;
            Context.InBlockSize = SwParseFileSize(Argument);
            if (Context.InBlockSize == -1ULL) {
                SwPrintError(0, Argument, "Invalid block size");
                Status = EINVAL;
                goto MainEnd;
            }

        } else if (strstr(Argument, "if=") == Argument) {
            Argument += 3;
            InPath = Argument;

        } else if (strstr(Argument, "iflag=") == Argument) {
            Argument += 6;
            Status = DdParseFileArguments(&Context, Argument, TRUE);
            if (Status != 0) {
                SwPrintError(Status, Argument, "Invalid file argument");
                goto MainEnd;
            }

        } else if (strstr(Argument, "obs=") == Argument) {
            Argument += 4;
            Context.OutBlockSize = SwParseFileSize(Argument);
            if (Context.OutBlockSize == -1ULL) {
                SwPrintError(0, Argument, "Invalid block size");
                Status = EINVAL;
                goto MainEnd;
            }

        } else if (strstr(Argument, "of=") == Argument) {
            Argument += 3;
            OutPath = Argument;

        } else if (strstr(Argument, "oflag=") == Argument) {
            Argument += 6;
            Status = DdParseFileArguments(&Context, Argument, FALSE);
            if (Status != 0) {
                SwPrintError(Status, Argument, "Invalid file argument");
                goto MainEnd;
            }

        } else if (strstr(Argument, "seek=") == Argument) {
            Argument += 5;
            Context.OutSkip = SwParseFileSize(Argument);
            if (Context.OutSkip == -1ULL) {
                SwPrintError(0, Argument, "Invalid size");
                Status = EINVAL;
                goto MainEnd;
            }

        } else if (strstr(Argument, "skip=") == Argument) {
            Argument += 5;
            Context.InSkip = SwParseFileSize(Argument);
            if (Context.InSkip == -1ULL) {
                SwPrintError(0, Argument, "Invalid size");
                Status = EINVAL;
                goto MainEnd;
            }

        } else {
            SwPrintError(0, Argument, "Unrecognized specification");
            Status = EINVAL;
            goto MainEnd;
        }

        ArgumentIndex += 1;
    }

    //
    // Consider implementing block and unblock if the masses are clamoring for
    // it.
    //

    if ((Context.Options & (DD_OPTION_BLOCK | DD_OPTION_UNBLOCK)) != 0) {
        SwPrintError(0, NULL, "Block/unblock modes currently not implemented");
        Status = ENOSYS;
        goto MainEnd;
    }

    //
    // Allocate a buffer.
    //

    BufferSize = Context.InBlockSize;
    if (BufferSize < Context.OutBlockSize) {
        BufferSize = Context.OutBlockSize;
    }

    if (BufferSize > MAX_UINTN) {
        SwPrintError(0, NULL, "Size %lld is too big", BufferSize);
        Status = ERANGE;
        goto MainEnd;
    }

    Buffer = malloc(BufferSize);
    if (Buffer == NULL) {
        SwPrintError(0, NULL, "Allocation failure");
        Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Multiply up the block sizes if needed.
    //

    if ((Context.Options & DD_OPTION_COUNT_BYTES) == 0) {
        Context.Count *= Context.InBlockSize;
    }

    if ((Context.Options & DD_OPTION_SKIP_BYTES) == 0) {
        Context.InSkip *= Context.InBlockSize;
    }

    if ((Context.Options & DD_OPTION_SEEK_BYTES) == 0) {
        Context.OutSkip *= Context.OutBlockSize;
    }

    SwGetMonotonicClock(&(Context.StartTime));

    //
    // Open up the files if specified.
    //

    if (InPath != NULL) {
        Input = SwOpen(InPath, Context.InOpenFlags, 0);
        if (Input < 0) {
            Status = errno;
            SwPrintError(Status, InPath, "Cannot open");
            goto MainEnd;
        }

    } else {
        InPath = "standard in";
        Input = STDIN_FILENO;
    }

    if (OutPath != NULL) {
        Output = SwOpen(OutPath,
                        Context.OutOpenFlags,
                        DD_DEFAULT_CREATION_MASK);

        if (Output < 0) {
            Status = errno;
            SwPrintError(Status, OutPath, "Cannot open");
            goto MainEnd;
        }

    } else {
        OutPath = "standard out";
        Output = STDOUT_FILENO;
    }

    //
    // Skip over the beginning input and output.
    //

    if (Context.InSkip != 0) {
        if (lseek(Input, Context.InSkip, SEEK_CUR) < 0) {
            Count = Context.InSkip;
            while (Count != 0) {
                BytesThisRound = Context.InBlockSize;
                if (BytesThisRound > Count) {
                    BytesThisRound = Count;
                }

                do {
                    BytesComplete = read(Input, Buffer, BytesThisRound);

                } while ((BytesComplete < 0) && (errno == EINTR));

                if (BytesComplete < 0) {
                    Status = errno;
                    SwPrintError(Status, InPath, "Failed to read during skip");
                    goto MainEnd;
                }

                if (BytesComplete == 0) {
                    break;
                }

                Count -= BytesComplete;
            }
        }
    }

    if (Context.OutSkip != 0) {
        if (lseek(Output, Context.OutSkip, SEEK_CUR) < 0) {
            Status = errno;
            SwPrintError(Status, OutPath, "Failed to read during seek");
            goto MainEnd;
        }
    }

    //
    // Loop processing data.
    //

    while ((Context.Count == 0) || (Context.BytesComplete < Context.Count)) {
        if ((Context.Options & DD_OPTION_SYNC) != 0) {
            if ((Context.Options &
                 (DD_OPTION_BLOCK | DD_OPTION_UNBLOCK)) != 0) {

                memset(Buffer, ' ', Context.InBlockSize);

            } else {
                memset(Buffer, 0, Context.InBlockSize);
            }
        }

        //
        // Read a block.
        //

        BytesThisRound = Context.InBlockSize;
        if ((Context.Count != 0) &&
            (Context.Count - Context.BytesComplete < BytesThisRound)) {

            BytesThisRound = Context.Count - Context.BytesComplete;
        }

        do {
            if (Context.PrintRequest != FALSE) {
                Context.PrintRequest = FALSE;
                DdPrintIoStatistics(&Context);
            }

            if (Context.Exit != FALSE) {
                Status = EINTR;
                goto MainEnd;
            }

            BytesComplete = read(Input, Buffer, BytesThisRound);

        } while ((BytesComplete < 0) && (errno == EINTR));

        if (BytesComplete < 0) {
            Status = errno;
            SwPrintError(Status, InPath, "Failed to read");
            if ((Context.Options & DD_OPTION_NO_ERROR) == 0) {
                goto MainEnd;
            }

            DdPrintIoStatistics(&Context);

            //
            // Try to seek past the problem.
            //

            if (lseek(Input, Context.InBlockSize, SEEK_CUR) < 0) {
                Status = errno;
                SwPrintError(Status, InPath, "Also failed to seek");
            }

            if ((Context.Options & DD_OPTION_SYNC) == 0) {
                continue;
            }

            Context.BytesComplete += BytesThisRound;
            Context.InWholeBlocks += 1;

        } else if (BytesComplete == 0) {
            break;

        } else if (BytesComplete == BytesThisRound) {
            Context.InWholeBlocks += 1;
            Context.BytesComplete += BytesComplete;

        } else {
            Context.InPartialBlocks += 1;

            //
            // Sync just acts like the whole block was read.
            //

            if ((Context.Options & DD_OPTION_SYNC) != 0) {
                Context.BytesComplete += BytesThisRound;

            } else {
                Context.BytesComplete += BytesComplete;
            }
        }

        //
        // Perform conversions.
        //

        if ((Context.Options & DD_OPTION_SWAB) != 0) {
            for (ByteIndex = 0;
                 ByteIndex + 1 < BytesComplete;
                 ByteIndex += 2) {

                Swap = Buffer[ByteIndex];
                Buffer[ByteIndex] = Buffer[ByteIndex + 1];
                Buffer[ByteIndex + 1] = Swap;
            }
        }

        if ((Context.Options & DD_OPTION_LOWERCASE) != 0) {
            for (ByteIndex = 0; ByteIndex < BytesComplete; ByteIndex += 1) {
                Buffer[ByteIndex] = tolower(Buffer[ByteIndex]);
            }
        }

        if ((Context.Options & DD_OPTION_UPPERCASE) != 0) {
            for (ByteIndex = 0; ByteIndex < BytesComplete; ByteIndex += 1) {
                Buffer[ByteIndex] = toupper(Buffer[ByteIndex]);
            }
        }

        //
        // Skip the write if it's sparse and empty.
        //

        if ((Context.Options & DD_OPTION_SPARSE) != 0) {
            for (ByteIndex = 0; ByteIndex < BytesComplete; ByteIndex += 1) {
                if (Buffer[ByteIndex] != 0) {
                    break;
                }
            }

            if (ByteIndex == BytesComplete) {
                if (lseek(Output, BytesComplete, SEEK_CUR) >= 0) {
                    continue;

                } else {
                    Status = errno;
                    SwPrintError(Status, OutPath, "Seek error");
                }
            }
        }

        if (Context.PrintRequest != FALSE) {
            Context.PrintRequest = FALSE;
            DdPrintIoStatistics(&Context);
        }

        if (Context.Exit != FALSE) {
            Status = EINTR;
            goto MainEnd;
        }

        //
        // Write the block out.
        //

        BytesThisRound = BytesComplete;
        do {
            if (Context.PrintRequest != FALSE) {
                Context.PrintRequest = FALSE;
                DdPrintIoStatistics(&Context);
            }

            if (Context.Exit != FALSE) {
                Status = EINTR;
                goto MainEnd;
            }

            BytesComplete = write(Output, Buffer, BytesThisRound);

        } while ((BytesComplete < 0) && (errno == EINTR));

        if (BytesComplete < 0) {
            Status = errno;
            SwPrintError(Status, OutPath, "Write error");
            TotalStatus = 1;

        } else {
            if (BytesComplete == Context.OutBlockSize) {
                Context.OutWholeBlocks += 1;

            } else {
                Context.OutPartialBlocks += 1;
            }
        }
    }

    DdPrintIoStatistics(&Context);
    Status = 0;

MainEnd:
    sigaction(SIGINT, &OriginalSigint, NULL);
    sigaction(SIGUSR1, &OriginalSigusr1, NULL);
    if (Input > STDIN_FILENO) {
        close(Input);
    }

    if (Output > STDOUT_FILENO) {
        close(Output);
    }

    DdContext = NULL;
    if ((Status != 0) && (TotalStatus == 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

void
DdSignalHandler (
    int Signal
    )

/*++

Routine Description:

    This routine handles a SIGINT or SIGUSR1 signal while running the DD
    command.

Arguments:

    Signal - Supplies the signal number that fired.

Return Value:

    None.

--*/

{

    PDD_CONTEXT Context;

    Context = DdContext;
    if (Context == NULL) {
        fprintf(stderr, "dd: Bad signal timing\n");
        return;
    }

    assert((Signal == SIGINT) || (Signal == SIGUSR1));

    Context->PrintRequest = TRUE;
    if (Signal == SIGINT) {
        Context->Exit = TRUE;
    }

    return;
}

VOID
DdPrintIoStatistics (
    PDD_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints I/O statistics for the dd utility.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    double Rate;
    double Seconds;
    struct timespec Time;
    PSTR Unit;

    SwGetMonotonicClock(&Time);
    fprintf(stderr,
            "%llu+%llu records in\n%llu+%llu records out\n",
            Context->InWholeBlocks,
            Context->InPartialBlocks,
            Context->OutWholeBlocks,
            Context->OutPartialBlocks);

    Unit = "B";
    Seconds = (double)(Time.tv_sec - Context->StartTime.tv_sec);
    if (Seconds < 3600 * 24) {
        Seconds += (Time.tv_nsec - Context->StartTime.tv_nsec) /
                   1000000000.0;
    }

    Rate = (double)(Context->BytesComplete) / Seconds;
    if (Rate > 1024) {
        Rate /= 1024.0;
        Unit = "kB";
        if (Rate > 1024) {
            Rate /= 1024.0;
            Unit = "MB";
            if (Rate > 1024) {
                Rate /= 1024.0;
                Unit = "GB";
            }
        }
    }

    fprintf(stderr, "%f seconds, %f%s/s\n", Seconds, Rate, Unit);
    return;
}

INT
DdParseConversionArguments (
    PDD_CONTEXT Context,
    PSTR Argument
    )

/*++

Routine Description:

    This routine processes the conv= comma-separated arguments for the dd
    utility.

Arguments:

    Context - Supplies a pointer to the application context.

    Argument - Supplies the comma-separated conversion list.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    while (*Argument != '\0') {
        if (strstr(Argument, "ascii") == Argument) {
            SwPrintError(0, NULL, "Not supported");
            return EINVAL;

        } else if (strstr(Argument, "ebcdic") == Argument) {
            SwPrintError(0, NULL, "Not supported");
            return EINVAL;

        } else if (strstr(Argument, "ibm") == Argument) {
            SwPrintError(0, NULL, "Not supported");
            return EINVAL;

        } else if (strstr(Argument, "block") == Argument) {
            Argument += 5;
            Context->Options |= DD_OPTION_BLOCK;
            Context->Options &= ~DD_OPTION_UNBLOCK;

        } else if (strstr(Argument, "unblock") == Argument) {
            Argument += 7;
            Context->Options |= DD_OPTION_UNBLOCK;
            Context->Options &= ~DD_OPTION_BLOCK;

        } else if (strstr(Argument, "lcase") == Argument) {
            Argument += 5;
            Context->Options |= DD_OPTION_LOWERCASE;

        } else if (strstr(Argument, "ucase") == Argument) {
            Argument += 5;
            Context->Options |= DD_OPTION_UPPERCASE;

        } else if (strstr(Argument, "sparse") == Argument) {
            Argument += 6;
            Context->Options |= DD_OPTION_SPARSE;

        } else if (strstr(Argument, "swab") == Argument) {
            Argument += 4;
            Context->Options |= DD_OPTION_SWAB;

        } else if (strstr(Argument, "sync") == Argument) {
            Argument += 4;
            Context->Options |= DD_OPTION_SYNC;

        } else if (strstr(Argument, "excl") == Argument) {
            Argument += 4;
            Context->OutOpenFlags |= O_EXCL;

        } else if (strstr(Argument, "nocreat") == Argument) {
            Argument += 7;
            Context->OutOpenFlags &= ~O_CREAT;

        } else if (strstr(Argument, "notrunc") == Argument) {
            Argument += 7;
            Context->OutOpenFlags &= ~O_TRUNC;

        } else if (strstr(Argument, "noerror") == Argument) {
            Argument += 7;
            Context->Options |= DD_OPTION_NO_ERROR;

        } else {
            SwPrintError(0, Argument, "Unknown option");
            return EINVAL;
        }

        if (*Argument != '\0') {
            if (*Argument != ',') {
                return EINVAL;
            }

            Argument += 1;
        }
    }

    if (((Context->Options & DD_OPTION_UPPERCASE) != 0) &&
        ((Context->Options & DD_OPTION_LOWERCASE) != 0)) {

        SwPrintError(0, NULL, "Cannot combine lowercase and uppercase");
        return EINVAL;
    }

    return 0;
}

INT
DdParseFileArguments (
    PDD_CONTEXT Context,
    PSTR Argument,
    BOOL IsInput
    )

/*++

Routine Description:

    This routine processes the iflag= and oflag= comma-separated arguments for
    the dd utility.

Arguments:

    Context - Supplies a pointer to the application context.

    Argument - Supplies the comma-separated flag list.

    IsInput - Supplies a boolean indicating whether this is the input flags
        being parsed (TRUE) or the output flags (FALSE).

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT NewFlags;

    NewFlags = 0;
    while (*Argument != '\0') {
        if (strstr(Argument, "append") == Argument) {
            Argument += 6;
            NewFlags |= O_APPEND;

        } else if (strstr(Argument, "directory") == Argument) {
            Argument += 9;
            NewFlags |= O_DIRECTORY;

        } else if (strstr(Argument, "dsync") == Argument) {
            Argument += 5;
            NewFlags |= O_DSYNC;

        } else if (strstr(Argument, "sync") == Argument) {
            Argument += 4;
            NewFlags |= O_SYNC;

        } else if (strstr(Argument, "nonblock") == Argument) {
            Argument += 8;
            NewFlags |= O_NONBLOCK;

        } else if (strstr(Argument, "noatime") == Argument) {
            Argument += 7;
            NewFlags |= O_NOATIME;

        } else if (strstr(Argument, "noctty") == Argument) {
            Argument += 6;
            NewFlags |= O_NOCTTY;

        } else if (strstr(Argument, "nofollow") == Argument) {
            Argument += 8;
            NewFlags |= O_NOFOLLOW;

        } else {

            //
            // Handle input-specific flags.
            //

            if (IsInput != FALSE) {
                if (strstr(Argument, "fullblock") == Argument) {
                    Argument += 9;
                    Context->Options |= DD_OPTION_FULL_BLOCKS;

                } else if (strstr(Argument, "count_bytes") == Argument) {
                    Argument += 11;
                    Context->Options |= DD_OPTION_COUNT_BYTES;

                } else if (strstr(Argument, "skip_bytes") == Argument) {
                    Argument += 10;
                    Context->Options |= DD_OPTION_SKIP_BYTES;

                } else {
                    SwPrintError(0, Argument, "Unknown option");
                    return EINVAL;
                }

            //
            // Handle output-specific flags.
            //

            } else {
                if (strstr(Argument, "seek_bytes") == Argument) {
                    Argument += 10;
                    Context->Options |= DD_OPTION_SEEK_BYTES;

                } else {
                    SwPrintError(0, Argument, "Unknown option");
                    return EINVAL;
                }
            }
        }

        if (*Argument != '\0') {
            if (*Argument != ',') {
                return EINVAL;
            }

            Argument += 1;
        }
    }

    if (IsInput != FALSE) {
        Context->InOpenFlags |= NewFlags;

    } else {
        Context->OutOpenFlags |= NewFlags;
    }

    return 0;
}


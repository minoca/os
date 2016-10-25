/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    split.c

Abstract:

    This module implements the split utility, which reads an input file and
    writes zero or more output files limited in size.

Author:

    Evan Green 27-Jul-2014

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SPLIT_VERSION_MAJOR 1
#define SPLIT_VERSION_MINOR 0

#define SPLIT_USAGE                                                            \
    "usage: split [-l line_count] [-a suffix_length] [file [name]]\n"          \
    "       split -b n [-a suffix_length] [file [name]]\n"                     \
    "The split utility reads an input file and writes zero or more output \n"  \
    "files limited in size with a suffix. The suffix increments in the form \n"\
    "aa, ab, ac, ... ba, bb, ... zx, zy, zz. Options are:\n"                   \
    "  -a, --suffix-length=N -- Use output file name suffixes of length N. \n" \
    "      The default is 2.\n"                                                \
    "  -b, --bytes=size -- Put at most size bytes per output file.\n"          \
    "  -d, --numeric-suffixes -- Use numeric suffixes instead of alphabetic.\n"\
    "  -l, --lines=number -- Put number lines per output file.\n"              \
    "  -v, --verbose -- Output a message just before opening an output file.\n"\
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"     \

#define SPLIT_OPTIONS_STRING "a:b:dl:v"

//
// Define the default suffix length and output file name.
//

#define SPLIT_DEFAULT_SUFFIX_LENGTH 2
#define SPLIT_DEFAULT_OUTPUT_NAME "x"
#define SPLIT_DEFAULT_LINE_COUNT 1000

//
// Define split options.
//

//
// This option is set if the input should be split by bytes.
//

#define SPLIT_OPTION_BYTES 0x00000001

//
// This option is set if the input should be split by lines.
//

#define SPLIT_OPTION_LINES 0x00000002

//
// This option is set if the suffixes should be numeric instead of alphabetic.
//

#define SPLIT_OPTION_NUMERIC 0x00000004

//
// This option is set to output a diagnostic message just before opening an
// output file.
//

#define SPLIT_OPTION_VERBOSE 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option SplitLongOptions[] = {
    {"suffix-length", required_argument, 0, 'a'},
    {"bytes", required_argument, 0, 'b'},
    {"numeric-suffixes", no_argument, 0, 'd'},
    {"lines", required_argument, 0, 'l'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
SplitMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the split utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    ULONG ArgumentIndex;
    PSTR Buffer;
    UINTN BufferSize;
    ssize_t BytesRead;
    ssize_t BytesThisRound;
    ssize_t BytesWritten;
    FILE *Input;
    PSTR InputName;
    INT Option;
    ULONG Options;
    FILE *Output;
    PSTR OutputName;
    PSTR OutputPrefix;
    INT OutputPrefixLength;
    UINTN SplitCurrentSize;
    UINTN SplitSize;
    int Status;
    INT SuffixIndex;
    LONG SuffixLength;
    CHAR SuffixMax;
    CHAR SuffixMin;
    ssize_t TotalBytesWritten;

    Buffer = NULL;
    BufferSize = 0;
    Input = NULL;
    InputName = NULL;
    Options = 0;
    Output = NULL;
    OutputName = NULL;
    OutputPrefix = SPLIT_DEFAULT_OUTPUT_NAME;
    SplitSize = SPLIT_DEFAULT_LINE_COUNT;
    SuffixLength = SPLIT_DEFAULT_SUFFIX_LENGTH;
    SuffixMin = 'a';
    SuffixMax = 'z';

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SPLIT_OPTIONS_STRING,
                             SplitLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'a':
            SuffixLength = strtoul(optarg, &AfterScan, 10);
            if ((AfterScan == optarg) || (SuffixLength <= 0)) {
                SwPrintError(0, optarg, "Invalid suffix length");
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'b':
            Options |= SPLIT_OPTION_BYTES;
            SplitSize = strtoull(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid byte count");
                Status = EINVAL;
                goto MainEnd;
            }

            if (*AfterScan == 'K') {
                if (*(AfterScan + 1) == 'B') {
                    SplitSize *= 1000ULL;

                } else {
                    SplitSize *= 1024ULL;
                }

            } else if (*AfterScan == 'M') {
                if (*(AfterScan + 1) == 'B') {
                    SplitSize *= 1000ULL * 1000ULL;

                } else {
                    SplitSize *= 1024ULL * 1024ULL;
                }

            } else if (*AfterScan == 'G') {
                SplitSize *= 1024ULL * 1024ULL * 1024ULL;

            } else if (*AfterScan == 'T') {
                SplitSize *= 1024ULL * 1024ULL * 1024ULL * 1024ULL;

            } else if (*AfterScan == 'P') {
                SplitSize *= 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

            } else if (*AfterScan == 'E') {
                SplitSize *= 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL *
                             1024ULL;

            } else if (*AfterScan == 'Z') {
                SplitSize *= 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL *
                             1024ULL * 1024ULL;

            } else if (*AfterScan == 'Y') {
                SplitSize *= 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL *
                             1024ULL * 1024ULL * 1024ULL;
            }

            break;

        case 'd':
            Options |= SPLIT_OPTION_NUMERIC;
            SuffixMin = '0';
            SuffixMax = '9';
            break;

        case 'l':
            Options |= SPLIT_OPTION_LINES;
            SplitSize = strtoull(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid line count");
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'v':
            Options |= SPLIT_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(SPLIT_VERSION_MAJOR, SPLIT_VERSION_MINOR);
            return 1;

        case 'h':
            printf(SPLIT_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex < ArgumentCount) {
        InputName = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
    }

    if (ArgumentIndex < ArgumentCount) {
        OutputPrefix = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
    }

    if (ArgumentIndex < ArgumentCount) {
        SwPrintError(0, Arguments[ArgumentIndex], "Extra operand");
        Status = EINVAL;
        goto MainEnd;
    }

    if (((Options & SPLIT_OPTION_BYTES) != 0) &&
        ((Options & SPLIT_OPTION_LINES) != 0)) {

        SwPrintError(0, NULL, "Can't split in more than one way");
        Status = EINVAL;
        goto MainEnd;
    }

    if (((Options & SPLIT_OPTION_BYTES) == 0) &&
        ((Options & SPLIT_OPTION_LINES) == 0)) {

        Options |= SPLIT_OPTION_LINES;
    }

    //
    // Set up the output file name buffer.
    //

    OutputPrefixLength = strlen(OutputPrefix);
    OutputName = malloc(OutputPrefixLength + SuffixLength + 1);
    if (OutputName == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    strcpy(OutputName, OutputPrefix);
    for (SuffixIndex = 0; SuffixIndex < SuffixLength; SuffixIndex += 1) {
        OutputName[OutputPrefixLength + SuffixIndex] = SuffixMin;
    }

    OutputName[OutputPrefixLength + SuffixLength] = '\0';

    //
    // Open up the input.
    //

    if ((InputName == NULL) || (strcmp(InputName, "-") == 0)) {
        Input = stdin;

    } else {
        Input = fopen(InputName, "rb");
        if (Input == NULL) {
            Status = errno;
            SwPrintError(Status, InputName, "Cannot open");
            goto MainEnd;
        }
    }

    //
    // Create a buffer.
    //

    BufferSize = BUFSIZ;
    Buffer = malloc(BufferSize);
    if (Buffer == NULL) {
        Status = ENOMEM;
        goto MainEnd;
    }

    //
    // Loop grabbing the input.
    //

    SplitCurrentSize = 0;
    while (TRUE) {

        //
        // Take a big old bite of data.
        //

        BytesRead = fread(Buffer, 1, BufferSize, Input);
        if (BytesRead == 0) {
            if (feof(Input) != 0) {
                break;
            }

            if (ferror(Input) != 0) {
                Status = errno;
                SwPrintError(Status, NULL, "Error reading input");
                goto MainEnd;
            }
        }

        //
        // Now loop writing bytes to the output.
        //

        TotalBytesWritten = 0;
        while (TotalBytesWritten < BytesRead) {

            assert(SplitCurrentSize <= SplitSize);

            //
            // Figure out how much of this buffer can be written to this file.
            //

            if ((Options & SPLIT_OPTION_BYTES) != 0) {
                BytesThisRound = BytesRead - TotalBytesWritten;
                if (SplitSize - SplitCurrentSize < BytesThisRound) {
                    BytesThisRound = SplitSize - SplitCurrentSize;
                }

                SplitCurrentSize += BytesThisRound;

            } else {

                assert((Options & SPLIT_OPTION_LINES) != 0);

                BytesThisRound = 0;
                while ((BytesThisRound < BytesRead - TotalBytesWritten) &&
                       (SplitCurrentSize < SplitSize)) {

                    if (Buffer[TotalBytesWritten + BytesThisRound] == '\n') {
                        SplitCurrentSize += 1;
                    }

                    BytesThisRound += 1;
                }
            }

            //
            // Open up the output if it hasn't been opened yet.
            //

            if (Output == NULL) {
                if ((Options & SPLIT_OPTION_VERBOSE) != 0) {
                    printf("Opening file '%s'\n", OutputName);
                }

                Output = fopen(OutputName, "wb");
                if (Output == NULL) {
                    Status = errno;
                    SwPrintError(Status, OutputName, "Cannot open");
                    goto MainEnd;
                }
            }

            //
            // Write the buffer to the file.
            //

            while (BytesThisRound != 0) {
                do {
                    BytesWritten = fwrite(Buffer + TotalBytesWritten,
                                          1,
                                          BytesThisRound,
                                          Output);

                } while ((BytesWritten == 0) && (errno == EINTR));

                if ((BytesWritten == 0) && (ferror(Output) != 0)) {
                    Status = errno;
                    SwPrintError(Status, NULL, "Error writing");
                    goto MainEnd;
                }

                BytesThisRound -= BytesWritten;
                TotalBytesWritten += BytesWritten;
            }

            //
            // If this just maxed out the file, close it and advance the file.
            //

            if (SplitCurrentSize >= SplitSize) {
                fclose(Output);
                Output = NULL;

                assert(SplitCurrentSize == SplitSize);

                SplitCurrentSize = 0;
                SuffixIndex = SuffixLength - 1;
                while (TRUE) {

                    //
                    // If this digit is 'z' or '9', carry over to the next
                    // digit.
                    //

                    if (OutputName[OutputPrefixLength + SuffixIndex] ==
                        SuffixMax) {

                        //
                        // Watch out for running out of possible file names.
                        //

                        if (SuffixIndex == 0) {
                            SwPrintError(0,
                                         OutputName,
                                         "Ran out of suffixes");

                            Status = ERANGE;
                            goto MainEnd;
                        }

                        OutputName[OutputPrefixLength + SuffixIndex] =
                                                                     SuffixMin;

                        SuffixIndex -= 1;

                    //
                    // Just increment this digit, no carry.
                    //

                    } else {
                        OutputName[OutputPrefixLength + SuffixIndex] += 1;
                        break;
                    }
                }
            }
        }
    }

    Status = 0;

MainEnd:
    if (Output != NULL) {
        fclose(Output);
    }

    if ((Input != NULL) && (Input != stdin)) {
        fclose(Input);
    }

    if (OutputName != NULL) {
        free(OutputName);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//


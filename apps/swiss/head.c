/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    head.c

Abstract:

    This module implements the head utility.

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
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define HEAD_VERSION_MAJOR 1
#define HEAD_VERSION_MINOR 0

#define HEAD_USAGE                                                             \
    "usage: head [-c number | -n number] [files...]\n"                         \
    "The head command prints the first 10 or so lines to standard output.\n"   \
    "Options are:\n"                                                           \
    "  -c, --bytes=[-]number -- Output the first N bytes, or all but the \n"   \
    "      last N bytes with a - sign.\n"                                      \
    "  -n, --lines=[-]number -- Output the first N lines, or all but the \n"   \
    "      last N lines with a - sign.\n"                                      \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version - Show the application version information and exit.\n"

#define HEAD_OPTIONS_STRING "c:n:"

//
// Define head options.
//

//
// This option is set to count lines instead of bytes.
//

#define HEAD_OPTION_LINES 0x00000001

//
// This option is set to indicate that the offset is from the end of the file.
//

#define HEAD_OPTION_FROM_END 0x00000002

//
// This option is set to print file names as headers.
//

#define HEAD_OPTION_PRINT_NAMES 0x00000004

//
// Define the default offset.
//

#define HEAD_DEFAULT_OFFSET 10

//
// Define the maximum size of a "line" as far as keeping a buffer for it goes.
//

#define HEAD_MAX_LINE 2048

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
HeadProcessFile (
    PSTR FileName,
    ULONG Options,
    ULONGLONG Offset
    );

//
// -------------------------------------------------------------------- Globals
//

struct option HeadLongOptions[] = {
    {"bytes", required_argument, 0, 'c'},
    {"lines", required_argument, 0, 'n'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
HeadMain (
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

    PSTR Argument;
    ULONG ArgumentIndex;
    ULONGLONG Offset;
    INT Option;
    ULONG Options;
    INT Status;
    INT TotalStatus;

    Offset = HEAD_DEFAULT_OFFSET;
    Options = HEAD_OPTION_LINES;
    TotalStatus = 0;
    Status = 0;

    //
    // Handle something like head -40 myfile or head -4.
    //

    if (((ArgumentCount == 2) || (ArgumentCount == 3)) &&
        (Arguments[1][0] == '-') && (isdigit(Arguments[1][1]))) {

        Offset = strtoull(Arguments[1] + 1, NULL, 10);
        ArgumentIndex = 2;

    } else {

        //
        // Process the control arguments.
        //

        while (TRUE) {
            Option = getopt_long(ArgumentCount,
                                 Arguments,
                                 HEAD_OPTIONS_STRING,
                                 HeadLongOptions,
                                 NULL);

            if (Option == -1) {
                break;
            }

            if ((Option == '?') || (Option == ':')) {
                Status = 1;
                goto MainEnd;
            }

            switch (Option) {
            case 'c':
            case 'n':
                if (Option == 'c') {
                    Options &= ~HEAD_OPTION_LINES;

                } else {
                    Options |= HEAD_OPTION_LINES;
                }

                Argument = optarg;

                assert(Argument != NULL);

                if (*Argument == '+') {
                    Options &= ~HEAD_OPTION_FROM_END;
                    Argument += 1;

                } else {
                    if (*Argument == '-') {
                        Options |= HEAD_OPTION_FROM_END;
                        Argument += 1;
                    }
                }

                Offset = SwParseFileSize(Argument);
                if ((Offset == -1ULL) || (Offset == 0)) {
                    SwPrintError(0, Argument, "Invalid size");
                    Status = EINVAL;
                    goto MainEnd;
                }

                break;

            case 'V':
                SwPrintVersion(HEAD_VERSION_MAJOR, HEAD_VERSION_MINOR);
                return 1;

            case 'h':
                printf(HEAD_USAGE);
                return 1;

            default:

                assert(FALSE);

                Status = 1;
                goto MainEnd;
            }
        }

        ArgumentIndex = optind;
    }

    if (ArgumentIndex == ArgumentCount) {
        TotalStatus = HeadProcessFile("-", Options, Offset);

    } else {
        if (ArgumentIndex + 1 < ArgumentCount) {
            Options |= HEAD_OPTION_PRINT_NAMES;
        }

        while (ArgumentIndex < ArgumentCount) {
            Status = HeadProcessFile(Arguments[ArgumentIndex], Options, Offset);
            if (Status != 0) {
                TotalStatus = Status;
            }

            ArgumentIndex += 1;
        }
    }

MainEnd:
    if ((Status != 0) && (TotalStatus == 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
HeadProcessFile (
    PSTR FileName,
    ULONG Options,
    ULONGLONG Offset
    )

/*++

Routine Description:

    This routine processes a file for the head utility.

Arguments:

    FileName - Supplies a pointer to the file path string.

    Options - Supplies the application options.

    Offset - Supplies the number of lines or bytes to print.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PUCHAR Buffer;
    UINTN BufferSize;
    INT Character;
    UINTN Count;
    BOOL GotSomething;
    UINTN InIndex;
    FILE *Input;
    UINTN OutIndex;
    int Status;

    Buffer = NULL;
    Input = NULL;

    //
    // Open up the file if one was specified, or use standard in (in which case
    // the follow option is ignored).
    //

    if (strcmp(FileName, "-") == 0) {
        Input = stdin;
        FileName = "standard input";

    } else {
        Input = fopen(FileName, "r");
        if (Input == NULL) {
            Status = errno;
            SwPrintError(Status, FileName, "Unable to open");
            goto ProcessFileEnd;
        }
    }

    if ((Options & HEAD_OPTION_PRINT_NAMES) != 0) {
        printf("==> %s <==\n", FileName);
    }

    assert(Offset != 0);

    //
    // If printing all but the last N bytes/lines, get a delay buffer set up.
    //

    if ((Options & HEAD_OPTION_FROM_END) != 0) {
        BufferSize = Offset;
        if ((Options & HEAD_OPTION_LINES) != 0) {

            //
            // The buffer needs to store the last N lines plus the current line
            // being read.
            //

            BufferSize += 1;
            BufferSize *= HEAD_MAX_LINE;
        }

        Buffer = malloc(BufferSize);
        if (Buffer == NULL) {
            Status = ENOMEM;
            goto ProcessFileEnd;
        }

        InIndex = 0;
        OutIndex = 0;
        Count = 0;
        if ((Options & HEAD_OPTION_LINES) != 0) {

            //
            // Read in the first N lines.
            //

            Character = 0;
            while ((InIndex < BufferSize) &&
                   (Count < Offset)) {

                Character = fgetc(Input);
                if (Character == EOF) {
                    break;
                }

                Buffer[InIndex] = Character;
                InIndex += 1;
                if (Character == '\n') {
                    Count += 1;
                }
            }

            //
            // If there are fewer than the specified number of lines, just
            // return.
            //

            if (Character == EOF) {
                Status = 0;
                goto ProcessFileEnd;
            }

            //
            // Loop reading in lines, and printing out the oldest line.
            //

            do {

                //
                // Add a line to the buffer.
                //

                GotSomething = FALSE;
                while (InIndex != OutIndex) {
                    Character = fgetc(Input);
                    if (Character == EOF) {
                        break;
                    }

                    Buffer[InIndex] = Character;
                    InIndex += 1;
                    if (InIndex == BufferSize) {
                        InIndex = 0;
                    }

                    GotSomething = TRUE;
                    if (Character == '\n') {
                        break;
                    }
                }

                if (InIndex == OutIndex) {
                    SwPrintError(0, FileName, "Line too long");
                    Status = EINVAL;
                    goto ProcessFileEnd;
                }

                if (GotSomething == FALSE) {
                    break;
                }

                //
                // Read a line out.
                //

                do {
                    Character = Buffer[OutIndex];
                    fputc(Character, stdout);
                    OutIndex += 1;
                    if (OutIndex == BufferSize) {
                        OutIndex = 0;
                    }

                } while ((Character != '\n') && (OutIndex != InIndex));

            } while (GotSomething != FALSE);

        //
        // Print all but the last N bytes. This is much easier.
        //

        } else {

            //
            // Fill the buffer with the first N bytes.
            //

            while (InIndex < BufferSize) {
                Character = fgetc(Input);
                if (Character == EOF) {
                    break;
                }

                Buffer[InIndex] = Character;
                InIndex += 1;
            }

            //
            // If there aren't N bytes in the file, just exit.
            //

            if (InIndex != BufferSize) {
                Status = 0;
                goto ProcessFileEnd;
            }

            InIndex = 0;

            //
            // Print the oldest byte, and read a new byte into its place.
            //

            while (TRUE) {
                fputc(Buffer[InIndex], stdout);
                Character = fgetc(Input);
                if (Character == EOF) {
                    break;
                }

                Buffer[InIndex] = Character;
                InIndex += 1;
                if (InIndex == BufferSize) {
                    InIndex = 0;
                }
            }
        }

    //
    // Print the first N lines or bytes. Easy street.
    //

    } else {
        if ((Options & HEAD_OPTION_LINES) != 0) {
            while (Offset != 0) {
                Character = fgetc(Input);
                if (Character == EOF) {
                    break;
                }

                fputc(Character, stdout);
                if (Character == '\n') {
                    Offset -= 1;
                }
            }

        } else {
            while (Offset != 0) {
                Character = fgetc(Input);
                if (Character == EOF) {
                    break;
                }

                fputc(Character, stdout);
                Offset -= 1;
            }
        }
    }

    Status = 0;

ProcessFileEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if ((Input != NULL) && (Input != stdin)) {
        fclose(Input);
    }

    return Status;
}


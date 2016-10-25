/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tail.c

Abstract:

    This module implements the tail utility.

Author:

    Evan Green 3-Jul-2013

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
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TAIL_VERSION_MAJOR 1
#define TAIL_VERSION_MINOR 0

#define TAIL_USAGE                                                             \
    "usage: tail [-f] [-c number | -n number] [file]\n"                        \
    "The tail command copies its input to standard output starting at the \n"  \
    "given position. Positions start with + to specify an offset from the \n"  \
    "beginning of the file, or - for offsets from the end of the file. \n"     \
    "Both lines and bytes start counting at 1, not 0. Specifying a number \n"  \
    "without a sign is the same as specifying a -. Valid options are:\n"       \
    "  -f, --follow -- If the input is a regular file or the operand \n"       \
    "        specifies a FIFO, do not terminate after the last line of the  \n"\
    "        input has been copied. Read and copy further bytes as they \n"    \
    "        become available. If no file operand is specified and standard \n"\
    "        in is a pipe, this option is ignored.\n"                          \
    "  -c, --bytes=number -- Output the first or last number of bytes, \n"     \
    "        depending on whether a + or - is prepended to the number.\n"      \
    "  -n, --lines=number -- Output the first or last number of lines.\n"      \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version - Show the application version information and exit.\n"

#define TAIL_OPTIONS_STRING "fc:n:"

//
// Define tail options.
//

//
// This option is set to continue printing even after the end of the file.
//

#define TAIL_OPTION_FOLLOW 0x00000001

//
// This option is set to count lines instead of bytes.
//

#define TAIL_OPTION_LINES 0x00000002

//
// This option is set to indicate that the offset is from the end of the file.
//

#define TAIL_OPTION_FROM_END 0x00000004

//
// Define the default offset.
//

#define TAIL_DEFAULT_OFFSET 10

//
// Define the maximum size of a "line" as far as keeping a buffer for it goes.
//

#define TAIL_MAX_LINE 2048

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option TailLongOptions[] = {
    {"follow", no_argument, 0, 'f'},
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
TailMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the tail utility.

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
    UINTN BackupCount;
    PUCHAR Buffer;
    UINTN BufferNextIndex;
    UINTN BufferSize;
    UINTN BufferValidSize;
    INT Character;
    PSTR FileName;
    FILE *Input;
    ULONGLONG Multiplier;
    ULONGLONG Offset;
    INT Option;
    ULONG Options;
    UINTN StartIndex;
    struct stat Stat;
    int Status;

    Buffer = NULL;
    Input = NULL;
    Multiplier = 1;
    Offset = TAIL_DEFAULT_OFFSET;
    Options = TAIL_OPTION_FROM_END | TAIL_OPTION_LINES;

    //
    // Handle something like tail -40 myfile or tail -4.
    //

    if (((ArgumentCount == 2) || (ArgumentCount == 3)) &&
        (Arguments[1][0] == '-') && (isdigit(Arguments[1][1]))) {

        Offset = strtoull(Arguments[1] + 1, NULL, 10);
        Options |= TAIL_OPTION_FROM_END;
        ArgumentIndex = 2;

    } else {

        //
        // Process the control arguments.
        //

        while (TRUE) {
            Option = getopt_long(ArgumentCount,
                                 Arguments,
                                 TAIL_OPTIONS_STRING,
                                 TailLongOptions,
                                 NULL);

            if (Option == -1) {
                break;
            }

            if ((Option == '?') || (Option == ':')) {
                Status = 1;
                goto MainEnd;
            }

            switch (Option) {
            case 'f':
                Options |= TAIL_OPTION_FOLLOW;
                break;

            case 'c':
            case 'n':
                if (Option == 'c') {
                    Options &= ~TAIL_OPTION_LINES;

                } else {
                    Options |= TAIL_OPTION_LINES;
                }

                Argument = optarg;

                assert(Argument != NULL);

                if (*Argument == '+') {
                    Options &= ~TAIL_OPTION_FROM_END;
                    Argument += 1;

                } else {
                    Options |= TAIL_OPTION_FROM_END;
                    if (*Argument == '-') {
                        Argument += 1;
                    }
                }

                Offset = SwParseFileSize(Argument);
                if (Offset == -1ULL) {
                    SwPrintError(0, Argument, "Invalid size");
                    Status = EINVAL;
                    goto MainEnd;
                }

                break;

            case 'V':
                SwPrintVersion(TAIL_VERSION_MAJOR, TAIL_VERSION_MINOR);
                return 1;

            case 'h':
                printf(TAIL_USAGE);
                return 1;

            default:

                assert(FALSE);

                Status = 1;
                goto MainEnd;
            }
        }

        ArgumentIndex = optind;
    }

    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    FileName = NULL;
    if (ArgumentIndex < ArgumentCount) {
        FileName = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
    }

    if (ArgumentIndex < ArgumentCount) {
        SwPrintError(0, Arguments[ArgumentIndex], "Unexpected operand");
        Status = EINVAL;
        goto MainEnd;
    }

    //
    // Open up the file if one was specified, or use standard in (in which case
    // the follow option is ignored).
    //

    if (FileName != NULL) {
        Input = fopen(FileName, "r");
        if (Input == NULL) {
            Status = errno;
            SwPrintError(Status, FileName, "Unable to open");
            goto MainEnd;
        }

    } else {
        FileName = "(stdin)";
        Input = stdin;
        Options &= ~TAIL_OPTION_FOLLOW;
    }

    assert(Offset != 0);

    if ((Options & TAIL_OPTION_FROM_END) == 0) {
        Offset -= 1;
    }

    Offset *= Multiplier;

    //
    // If it's a regular file in byte mode, use seek.
    //

    if ((Input != stdin) & ((Options & TAIL_OPTION_LINES) == 0)) {
        Status = SwOsStat(FileName, TRUE, &Stat);
        if ((Status == 0) && (S_ISREG(Stat.st_mode))) {
            if ((Options & TAIL_OPTION_FROM_END) != 0) {
                if (Offset > Stat.st_size) {
                    Offset = Stat.st_size;
                }

                Status = fseek(Input, Stat.st_size - Offset, SEEK_SET);

            } else {
                Status = fseek(Input, Offset, SEEK_SET);
            }

            if (Status == 0) {
                Offset = 0;
            }
        }
    }

    //
    // If there's still an offset, go to it.
    //

    if (Offset != 0) {
        if ((Options & TAIL_OPTION_FROM_END) != 0) {
            BufferSize = Offset;
            if ((Options & TAIL_OPTION_LINES) != 0) {
                BufferSize *= TAIL_MAX_LINE;
            }

            Buffer = malloc(BufferSize);
            if (Buffer == NULL) {
                Status = ENOMEM;
                goto MainEnd;
            }

            BufferNextIndex = 0;
            BufferValidSize = 0;

            //
            // Get to the end of the file.
            //

            while (TRUE) {
                Character = fgetc(Input);
                if (Character == EOF) {
                    break;
                }

                Buffer[BufferNextIndex] = Character;
                BufferNextIndex += 1;
                if (BufferNextIndex == BufferSize) {
                    BufferNextIndex = 0;
                }

                if (BufferValidSize < BufferSize) {
                    BufferValidSize += 1;
                }
            }

            //
            // Back up by the requested number of bytes or lines.
            //

            if ((Options & TAIL_OPTION_LINES) != 0) {
                BackupCount = 0;

                //
                // Start from the character before the very last character,
                // in order to skip a newline at the very end.
                //

                StartIndex = BufferNextIndex;
                if (StartIndex == 0) {
                    StartIndex = BufferSize - 1;

                } else {
                    StartIndex -= 1;
                }

                while (BackupCount < BufferValidSize) {
                    if (StartIndex == 0) {
                        StartIndex = BufferSize - 1;

                    } else {
                        StartIndex -= 1;
                    }

                    if (Buffer[StartIndex] == '\n') {
                        Offset -= 1;

                        //
                        // Go forward by one character at the end so everything
                        // doesn't begin with that newline.
                        //

                        if (Offset == 0) {
                            StartIndex += 1;
                            if (StartIndex == BufferSize) {
                                StartIndex = 0;
                            }

                            break;
                        }
                    }

                    BackupCount += 1;
                }

                //
                // Account for that last character that was skipped.
                //

                BackupCount += 1;

            } else {

                assert(BufferSize == Offset);

                if (BufferNextIndex < BufferValidSize) {
                    StartIndex = BufferNextIndex - BufferValidSize + BufferSize;

                } else {
                    StartIndex = BufferNextIndex - BufferValidSize;
                }

                BackupCount = BufferValidSize;
            }

            //
            // Print out the buffered contents.
            //

            while (BackupCount != 0) {
                fputc(Buffer[StartIndex], stdout);
                if (StartIndex == BufferSize - 1) {
                    StartIndex = 0;

                } else {
                    StartIndex += 1;
                }

                BackupCount -= 1;
            }

        //
        // Seek from the beginning.
        //

        } else {
            while (Offset != 0) {
                Character = fgetc(Input);
                if (Character == EOF) {
                    break;
                }

                if (((Options & TAIL_OPTION_LINES) == 0) ||
                    (Character == '\n')) {

                    Offset -= 1;
                }
            }
        }
    }

    //
    // Now the easy part, just print the contents.
    //

    Status = 0;
    while (TRUE) {
        Character = fgetc(Input);
        if (Character == EOF) {
            if (ferror(Input) != 0) {
                Status = errno;
                break;
            }

            //
            // If following and this is just the end of the file, sleep for a
            // second and try again.
            //

            if ((Options & TAIL_OPTION_FOLLOW)) {
                SwSleep(1000000);
                continue;
            }

            //
            // Otherwise, stop.
            //

            break;
        }

        fputc(Character, stdout);
    }

MainEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    if ((Input != NULL) && (Input != stdin)) {
        fclose(Input);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//


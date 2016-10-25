/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    wc.c

Abstract:

    This module implements the wc (word count) utility.

Author:

    Evan Green 21-Oct-2013

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

#define WC_VERSION_MAJOR 1
#define WC_VERSION_MINOR 0

#define WC_USAGE                                                               \
    "usage: wc [-c | -m] [-lw] [file...]\n"                                    \
    "The wc utility prints the number of newlines, words, and bytes in each \n"\
    "input file. If no options are specified, wc prints \n"                    \
    "\"<newlines> <words> <bytes> <file>\". Options are:\n"                    \
    "  -c, --bytes -- Print the number of bytes in each input file.\n"         \
    "  -m, --chars -- Print the number of characters in each input file.\n"    \
    "  -l, --lines -- Print the number of newline characters.\n"               \
    "  -L, --max-line-length -- Print the length of the longest line.\n"       \
    "  -w, --words -- Print the word counts.\n"                                \
    "  --help -- Print this help text and exit.\n"                             \
    "  --version -- Print the application version information and exit.\n"

#define WC_OPTIONS_STRING "cmlLw"

//
// Define word count options.
//

//
// Set this option to print the number of bytes.
//

#define WC_OPTION_PRINT_BYTES 0x00000001

//
// Set this option to print characters.
//

#define WC_OPTION_PRINT_CHARACTERS 0x00000002

//
// Set this option to print lines.
//

#define WC_OPTION_PRINT_LINES 0x00000004

//
// Set this option to print the maximum line length.
//

#define WC_OPTION_PRINT_MAX_LINE_LENGTH 0x00000008

//
// Set this option to print the word count.
//

#define WC_OPTION_PRINT_WORDS 0x00000010

//
// Define the default set of options if none is provided, which prints
// lines, words, and bytes.
//

#define WC_DEFAULT_OPTIONS \
    (WC_OPTION_PRINT_BYTES | WC_OPTION_PRINT_WORDS | WC_OPTION_PRINT_LINES)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
WcProcessInput (
    ULONG Options,
    FILE *Input,
    PSTR Name,
    PULONGLONG TotalBytes,
    PULONGLONG TotalCharacters,
    PULONGLONG MaxLineLength,
    PULONGLONG TotalLines,
    PULONGLONG TotalWords
    );

VOID
WcPrintResults (
    ULONG Options,
    PSTR Name,
    ULONGLONG TotalBytes,
    ULONGLONG TotalCharacters,
    ULONGLONG MaxLineLength,
    ULONGLONG TotalLines,
    ULONGLONG TotalWords
    );

//
// -------------------------------------------------------------------- Globals
//

struct option WcLongOptions[] = {
    {"bytes", no_argument, 0, 'c'},
    {"chars", no_argument, 0, 'm'},
    {"lines", no_argument, 0, 'l'},
    {"max-line-length", no_argument, 0, 'L'},
    {"words", no_argument, 0, 'w'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
WcMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the wc utility.

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
    FILE *Input;
    ULONGLONG MaxLineLength;
    INT Option;
    ULONG Options;
    ULONG ProcessedCount;
    int Status;
    ULONGLONG TotalBytes;
    ULONGLONG TotalCharacters;
    ULONGLONG TotalLines;
    INT TotalStatus;
    ULONGLONG TotalWords;

    MaxLineLength = 0;
    ProcessedCount = 0;
    TotalBytes = 0;
    TotalCharacters = 0;
    TotalLines = 0;
    TotalStatus = 0;
    TotalWords = 0;
    Options = 0;
    Status = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             WC_OPTIONS_STRING,
                             WcLongOptions,
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
            Options |= WC_OPTION_PRINT_BYTES;
            Options &= ~WC_OPTION_PRINT_CHARACTERS;
            break;

        case 'm':
            Options |= WC_OPTION_PRINT_CHARACTERS;
            Options &= ~WC_OPTION_PRINT_BYTES;
            break;

        case 'l':
            Options |= WC_OPTION_PRINT_LINES;
            break;

        case 'L':
            Options |= WC_OPTION_PRINT_MAX_LINE_LENGTH;
            break;

        case 'w':
            Options |= WC_OPTION_PRINT_WORDS;
            break;

        case 'V':
            SwPrintVersion(WC_VERSION_MAJOR, WC_VERSION_MINOR);
            return 1;

        case 'h':
            printf(WC_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (Options == 0) {
        Options = WC_DEFAULT_OPTIONS;
    }

    //
    // If there are no arguments, use standard in.
    //

    if (ArgumentIndex == ArgumentCount) {
        Status = WcProcessInput(Options,
                                stdin,
                                "",
                                &TotalBytes,
                                &TotalCharacters,
                                &MaxLineLength,
                                &TotalLines,
                                &TotalWords);

        goto MainEnd;
    }

    //
    // Loop through the arguments again and process the input.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        if (strcmp(Argument, "-") == 0) {
            Input = stdin;

        } else {
            Input = fopen(Argument, "rb");
            if (Input == NULL) {
                TotalStatus = errno;
                SwPrintError(TotalStatus, Argument, "Unable to open");
                continue;
            }
        }

        Status = WcProcessInput(Options,
                                Input,
                                Argument,
                                &TotalBytes,
                                &TotalCharacters,
                                &MaxLineLength,
                                &TotalLines,
                                &TotalWords);

        if (Input != stdin) {
            fclose(Input);
        }

        if ((Status != 0) && (TotalStatus == 0)) {
            TotalStatus = Status;
        }

        ProcessedCount += 1;
    }

    //
    // Finally, print the total if more than one file was processed.
    //

    if (ProcessedCount > 1) {
        WcPrintResults(Options,
                       "total",
                       TotalBytes,
                       TotalCharacters,
                       MaxLineLength,
                       TotalLines,
                       TotalWords);
    }

MainEnd:
    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
WcProcessInput (
    ULONG Options,
    FILE *Input,
    PSTR Name,
    PULONGLONG TotalBytes,
    PULONGLONG TotalCharacters,
    PULONGLONG MaxLineLength,
    PULONGLONG TotalLines,
    PULONGLONG TotalWords
    )

/*++

Routine Description:

    This routine is called to gather the statistics on an input file for the
    wc utility.

Arguments:

    Options - Supplies the bitfield of application options. See WC_OPTION_*
        definitions.

    Input - Supplies a pointer to the input file to gather statistics on.

    Name - Supplies a pointer to the string containing the name to use when
        printing the results.

    TotalBytes - Supplies a pointer where the total bytes will be updated.

    TotalCharacters - Supplies a pointer where the total characters will be
        updated.

    MaxLineLength - Supplies a pointer where the maximum line length will be
        updated.

    TotalLines - Supplies a pointer where the total line count will be updated.

    TotalWords - Supplies a pointer where the total number of words will be
        returned.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    ULONGLONG BiggestLineLength;
    ULONGLONG Bytes;
    INT Character;
    ULONGLONG Characters;
    ULONGLONG LineLength;
    ULONGLONG Lines;
    INT Status;
    BOOL WasSpace;
    ULONGLONG Words;

    BiggestLineLength = 0;
    Bytes = 0;
    Characters = 0;
    LineLength = 0;
    Lines = 0;
    Status = 0;
    WasSpace = TRUE;
    Words = 0;
    while (TRUE) {
        Character = fgetc(Input);
        if (Character == EOF) {
            break;
        }

        Bytes += 1;
        Characters += 1;

        //
        // Count words. A non-space preceded by a space marks a new word.
        //

        if (isspace(Character)) {
            WasSpace = TRUE;

        } else {
            if (WasSpace != FALSE) {
                Words += 1;
            }

            WasSpace = FALSE;
        }

        //
        // Count newlines, and at the same time keep track of the longest line.
        //

        if (Character == '\n') {
            Lines += 1;
            if (LineLength > BiggestLineLength) {
                BiggestLineLength = LineLength;
            }

            LineLength = 0;

        } else {
            LineLength += 1;
        }
    }

    if (ferror(Input) != 0) {
        Status = errno;
        SwPrintError(Status, Name, "Failed to read");
    }

    *TotalBytes += Bytes;
    *TotalCharacters += Characters;
    if (BiggestLineLength > *MaxLineLength) {
        *MaxLineLength = BiggestLineLength;
    }

    *TotalLines += Lines;
    *TotalWords += Words;
    WcPrintResults(Options,
                   Name,
                   Bytes,
                   Characters,
                   BiggestLineLength,
                   Lines,
                   Words);

    return Status;
}

VOID
WcPrintResults (
    ULONG Options,
    PSTR Name,
    ULONGLONG TotalBytes,
    ULONGLONG TotalCharacters,
    ULONGLONG MaxLineLength,
    ULONGLONG TotalLines,
    ULONGLONG TotalWords
    )

/*++

Routine Description:

    This routine prints the statistics for a single file in the word count
    utility.

Arguments:

    Options - Supplies the bitfield of application options. See WC_OPTION_*
        definitions.

    Name - Supplies a pointer to the string containing the name to use when
        printing the results.

    TotalBytes - Supplies the total number of bytes in the file.

    TotalCharacters - Supplies the number of characters in the file.

    MaxLineLength - Supplies the maximum line length in the file.

    TotalLines - Supplies the total number of lines in the file.

    TotalWords - Supplies the number of words in the file.

Return Value:

    None.

--*/

{

    PSTR Space;

    Space = "";
    if ((Options & WC_OPTION_PRINT_LINES) != 0) {
        printf("%7lld", TotalLines);
        Space = " ";
    }

    if ((Options & WC_OPTION_PRINT_WORDS) != 0) {
        printf("%s%7lld", Space, TotalWords);
        Space = " ";
    }

    if ((Options & WC_OPTION_PRINT_BYTES) != 0) {
        printf("%s%7lld", Space, TotalBytes);
        Space = " ";
    }

    if ((Options & WC_OPTION_PRINT_CHARACTERS) != 0) {
        printf("%s%7lld", Space, TotalCharacters);
        Space = " ";
    }

    if ((Options & WC_OPTION_PRINT_MAX_LINE_LENGTH) != 0) {
        printf("%s%7lld", Space, MaxLineLength);
        Space = " ";
    }

    if (*Name != '\0') {
        printf("%s%s\n", Space, Name);

    } else {
        printf("\n");
    }

    return;
}


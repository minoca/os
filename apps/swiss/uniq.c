/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    uniq.c

Abstract:

    This module implements the uniq utility, which removes adjacent duplicate
    lines.

Author:

    Evan Green 9-Sep-2013

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

#define UNIQ_VERSION_MAJOR 1
#define UNIQ_VERSION_MINOR 0

#define UNIQ_USAGE                                                             \
    "usage: uniq [-cdu] [-f fields] [-s char] [input_file [output_file]]\n"    \
    "The uniq utility reads an input file, comparing adjacent lines, and \n"   \
    "writes one unique copy of each input line to the output. The input and \n"\
    "output file operands are optional. If an input is not supplied or if \n"  \
    "it is -, then standard in will be used. Options are:\n"                   \
    "  -c, --count -- Precede each output line with the number of "            \
    "occurrences.\n"                                                           \
    "  -d, --repeated -- Suppress the writing of lines that are not \n"        \
    "        repeated in the input.\n"                                         \
    "  -f, --skip-fields N -- Avoid comparing the first N fields. Fields are\n"\
    "        separated by blanks.\n"                                           \
    "  -i, --ignore-case -- Ignore case when comparing.\n"                     \
    "  -s, --skip-chars N -- Avoid comparing the first N characters.\n"        \
    "  -u, --unique -- Suppress the writing of lines that are repeated in \n"  \
    "        the input.\n"                                                     \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Show the application version and exit.\n"                  \

#define UNIQ_OPTIONS_STRING "cdf:is:u"

//
// Define uniq options.
//

//
// Set this option to precede each output line with a count.
//

#define UNIQ_OPTION_PRINT_COUNT 0x00000001

//
// Set this flag to skip writing lines that are not repeated.
//

#define UNIQ_OPTION_SUPPRESS_UNIQUE 0x00000002

//
// Set this flag to ignore case when comparing.
//

#define UNIQ_OPTION_IGNORE_CASE 0x00000004

//
// Set this flag to suppress repeated lines in the input.
//

#define UNIQ_OPTION_SUPPRESS_REPEATED 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
UniqSkip (
    PSTR Input,
    ULONG FieldCount,
    ULONG CharacterCount
    );

//
// -------------------------------------------------------------------- Globals
//

struct option UniqLongOptions[] = {
    {"count", no_argument, 0, 'c'},
    {"repeated", no_argument, 0, 'd'},
    {"skip-fields", required_argument, 0, 'f'},
    {"ignore-case", no_argument, 0, 'i'},
    {"skip-chars", required_argument, 0, 's'},
    {"unique", no_argument, 0, 'u'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
UniqMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the cp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PSTR Argument;
    ULONG ArgumentIndex;
    INT Comparison;
    FILE *Input;
    PSTR InputName;
    PSTR Line;
    PSTR LineStart;
    INT Option;
    ULONG Options;
    FILE *Output;
    PSTR OutputName;
    PSTR PreviousLine;
    PSTR PreviousLineStart;
    BOOL PrintLine;
    ULONG RepeatCount;
    LONG SkipCharacters;
    LONG SkipFields;
    int Status;

    Input = NULL;
    Line = NULL;
    PreviousLine = NULL;
    Options = 0;
    Output = NULL;
    SkipCharacters = 0;
    SkipFields = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             UNIQ_OPTIONS_STRING,
                             UniqLongOptions,
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
            Options |= UNIQ_OPTION_PRINT_COUNT;
            break;

        case 'd':
            Options |= UNIQ_OPTION_SUPPRESS_UNIQUE;
            break;

        case 'i':
            Options |= UNIQ_OPTION_IGNORE_CASE;
            break;

        case 'u':
            Options |= UNIQ_OPTION_SUPPRESS_REPEATED;
            break;

        case 'f':
            Argument = optarg;

            assert(Argument != NULL);

            SkipFields = strtol(Argument, &AfterScan, 10);
            if ((SkipFields < 0) || (AfterScan == Argument)) {
                SwPrintError(0, Argument, "Invalid field count");
                return 1;
            }

            break;

        case 's':
            Argument = optarg;

            assert(Argument != NULL);

            SkipCharacters = strtol(Argument, &AfterScan, 10);
            if ((SkipCharacters < 0) || (AfterScan == Argument)) {
                SwPrintError(0, Argument, "Invalid character count");
                return 1;
            }

            break;

        case 'V':
            SwPrintVersion(UNIQ_VERSION_MAJOR, UNIQ_VERSION_MINOR, REVISION);
            return 1;

        case 'h':
            printf(UNIQ_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // Get the optional input and output names.
    //

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    InputName = NULL;
    OutputName = NULL;
    if (ArgumentIndex < ArgumentCount) {
        InputName = Arguments[ArgumentIndex];
        if (strcmp(InputName, "-") == 0) {
            InputName = NULL;
        }

        ArgumentIndex += 1;
        if (ArgumentIndex < ArgumentCount) {
            OutputName = Arguments[ArgumentIndex];
            ArgumentIndex += 1;
            if (ArgumentIndex < ArgumentCount) {
                SwPrintError(0, Arguments[ArgumentIndex], "Too many arguments");
                return 1;
            }
        }
    }

    if (InputName == NULL) {
        Input = stdin;

    } else {
        Input = fopen(InputName, "r");
        if (Input == NULL) {
            Status = errno;
            SwPrintError(Status, InputName, "Unable to open");
            goto MainEnd;
        }
    }

    if (OutputName == NULL) {
        Output = stdout;

    } else {
        Output = fopen(OutputName, "r");
        if (Output == NULL) {
            Status = errno;
            SwPrintError(Status, OutputName, "Unable to open");
            goto MainEnd;
        }
    }

    //
    // Loop processing the files.
    //

    Status = SwReadLine(Input, &PreviousLine);
    if ((Status != 0) || (PreviousLine == NULL)) {
        goto MainEnd;
    }

    RepeatCount = 1;
    while (TRUE) {
        if (feof(Input) != 0) {
            Line = NULL;
            Comparison = 1;

        } else {
            Status = SwReadLine(Input, &Line);
            if (Status != 0) {
                goto MainEnd;
            }

            if (Line == NULL) {
                Comparison = 1;

            } else {
                LineStart = UniqSkip(Line, SkipFields, SkipCharacters);
                PreviousLineStart = UniqSkip(PreviousLine,
                                             SkipFields,
                                             SkipCharacters);

                if ((Options & UNIQ_OPTION_IGNORE_CASE) != 0) {
                    Comparison = strcasecmp(LineStart, PreviousLineStart);

                } else {
                    Comparison = strcmp(LineStart, PreviousLineStart);
                }
            }
        }

        //
        // If they're equal, move on to the next line.
        //

        if (Comparison == 0) {
            free(Line);
            Line = NULL;
            RepeatCount += 1;
            continue;
        }

        //
        // They're not equal, so spit this line out.
        //

        PrintLine = TRUE;
        if (RepeatCount == 1) {
            if ((Options & UNIQ_OPTION_SUPPRESS_UNIQUE) != 0) {
                PrintLine = FALSE;
            }

        } else {
            if ((Options & UNIQ_OPTION_SUPPRESS_REPEATED) != 0) {
                PrintLine = FALSE;
            }
        }

        if (PrintLine != FALSE) {
            if ((Options & UNIQ_OPTION_PRINT_COUNT) != 0) {
                printf("%d %s\n", RepeatCount, PreviousLine);

            } else {
                printf("%s\n", PreviousLine);
            }
        }

        //
        // Move the current line to the previous line.
        //

        free(PreviousLine);
        PreviousLine = Line;
        RepeatCount = 1;
        if (Line == NULL) {
            break;
        }
    }

MainEnd:
    if (Line != NULL) {
        free(Line);
    }

    if (PreviousLine != NULL) {
        free(PreviousLine);
    }

    if ((Input != NULL) && (Input != stdin)) {
        fclose(Input);
    }

    if ((Output != NULL) && (Output != stdout)) {
        fclose(Output);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
UniqSkip (
    PSTR Input,
    ULONG FieldCount,
    ULONG CharacterCount
    )

/*++

Routine Description:

    This routine skips a certain number of fields and/or characters, where a
    field is defined as any number of blank spaces followed by any number of
    non-blank spaces. Fields are skipped before characters.

Arguments:

    Input - Supplies the input to advance.

    FieldCount - Supplies the number of fields to skip.

    CharacterCount - Supplies the number of characters to skip.

Return Value:

    Returns a pointer within the string advanced past the specified number of
    fields and/or characters..

--*/

{

    ULONG FieldIndex;

    for (FieldIndex = 0; FieldIndex < FieldCount; FieldIndex += 1) {
        if (*Input == '\0') {
            break;
        }

        while (isblank(*Input)) {
            Input += 1;
        }

        while (!isblank(*Input)) {
            Input += 1;
        }
    }

    while ((CharacterCount != 0) && (*Input != '\0')) {
        Input += 1;
        CharacterCount -= 1;
    }

    return Input;
}


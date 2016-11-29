/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cut.c

Abstract:

    This module implements the cut utility, which cuts out selected fields of
    each line of a file.

Author:

    Evan Green 26-Jul-2014

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

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CUT_VERSION_MAJOR 1
#define CUT_VERSION_MINOR 0

#define CUT_USAGE                                                              \
    "usage cut -b list [-n] [file...]\n"                                       \
    "      cut -c list [file...]\n"                                            \
    "      cut -f list [-d delimiter] [-s] [file...]\n"                        \
    "The cut utility prints selected parts of lines from the given files to \n"\
    "standard out. Exactly one of -b, -c, or -f should be specified. Valid \n" \
    "options are:\n"                                                           \
    "  -b, --bytes=list -- Select only the given bytes.\n"                     \
    "  -c, --characters=list -- Select only the given characters.\n"           \
    "  -d, --delimiter=character -- Use the given delimiter as a character. \n"\
    "      The default is tab.\n"                                              \
    "  -f, --fields=list -- Select only the given fields separated by the \n"  \
    "      delimiter. Also print any line that contains no delimiter, \n"      \
    "      unless -s is specified.\n"                                          \
    "  -n -- Ignored.\n"                                                       \
    "  --complement -- Invert the set of selected bytes, characters, or \n"    \
    "      fields.\n"                                                          \
    "  -s, --only-delimited -- Do not print lines not containing a \n"         \
    "      delimiter.\n"                                                       \
    "  --output-delimiter=string -- Use the given string as an output field \n"\
    "      delimiter. The default is to use the input delimiter.\n"            \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n\n"   \
    "Byte, character, and field lists should be a comma or blank separated \n" \
    "(but still in the same argument) list of one of the following formats:\n" \
    "  N -- Print the Nth byte/character/field.\n"                             \
    "  N-M -- Print the Nths through the Mth byte/character/field.\n"          \
    "  N- -- Print the Nth byte/character/field through the end of the line.\n"\
    "  -N -- Print the beginning of the line through the Nth character.\n"     \
    "Fields are output in the order they are read in, not the order they \n"   \
    "are specified in the list. If no file or - is specified, standard in is\n"\
    "read. Returns 0 on success, or non-zero on error.\n"                      \

#define CUT_OPTIONS_STRING "b:c:d:f:ns"

#define CUT_INITIAL_LINE_SIZE 512

//
// Define cut options.
//

//
// This option specifies byte mode.
//

#define CUT_OPTION_BYTE 0x00000001

//
// This option specifies characters.
//

#define CUT_OPTION_CHARACTER 0x00000002

//
// This option specifies fields.
//

#define CUT_OPTION_FIELD 0x00000004

//
// This option inverts the given byte, character, or field selection.
//

#define CUT_OPTION_COMPLEMENT 0x00000008

//
// This option suppresses printing of lines with no delimiters.
//

#define CUT_OPTION_ONLY_DELIMITED 0x00000010

//
// This option remembers if a an option specific to fields was used.
//

#define CUT_OPTION_FIELD_OPTION_SPECIFIED 0x00000020

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a sed write file entry.

Members:

    Start - Stores the start of the range of bytes, characters, or fields.

    End - Stores the end of the range of bytes, characters, or fields.

--*/

typedef struct _CUT_RANGE {
    LONG Start;
    LONG End;
} CUT_RANGE, *PCUT_RANGE;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
CutFile (
    FILE *Input,
    INT Options,
    CHAR Delimiter,
    PSTR OutputDelimiter,
    PCUT_RANGE Range,
    UINTN RangeSize,
    PSTR *LineBuffer,
    PUINTN LineBufferSize
    );

INT
CutReadLine (
    FILE *Input,
    PSTR *LineBuffer,
    PUINTN LineBufferSize,
    PBOOL LastLine
    );

BOOL
CutIsElementInRange (
    LONG Element,
    INT Options,
    PCUT_RANGE RangeArray,
    UINTN RangeArraySize
    );

INT
CutCreateRangeArray (
    PSTR RangeString,
    PCUT_RANGE *RangeArray,
    PUINTN RangeArraySize
    );

int
CutCompareCutRanges (
    const void *First,
    const void *Second
    );

//
// -------------------------------------------------------------------- Globals
//

struct option CutLongOptions[] = {
    {"bytes", required_argument, 0, 'b'},
    {"characters", required_argument, 0, 'c'},
    {"delimiter", required_argument, 0, 'd'},
    {"fields", required_argument, 0, 'f'},
    {"complement", no_argument, 0, 'C'},
    {"only-delimited", no_argument, 0, 's'},
    {"output-delimiter", required_argument, 0, 'D'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
CutMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the cut utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT ActionsSpecified;
    PSTR Argument;
    ULONG ArgumentIndex;
    PCUT_RANGE Array;
    UINTN ArraySize;
    CHAR Delimiter;
    FILE *Input;
    PSTR LineBuffer;
    UINTN LineBufferSize;
    PSTR ListString;
    INT Option;
    ULONG Options;
    PSTR OutputDelimiter;
    int Status;
    int TotalStatus;

    ActionsSpecified = 0;
    Array = NULL;
    Delimiter = '\t';
    LineBuffer = NULL;
    LineBufferSize = 0;
    ListString = NULL;
    Options = 0;
    OutputDelimiter = NULL;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CUT_OPTIONS_STRING,
                             CutLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'b':
            ActionsSpecified += 1;
            Options |= CUT_OPTION_BYTE;
            ListString = optarg;
            break;

        case 'c':
            ActionsSpecified += 1;
            Options |= CUT_OPTION_CHARACTER;
            ListString = optarg;
            break;

        case 'd':
            if (strlen(optarg) != 1) {
                SwPrintError(0,
                             NULL,
                             "The delimiter must be a single character");

                Status = 1;
                goto MainEnd;
            }

            Delimiter = optarg[0];
            Options |= CUT_OPTION_FIELD_OPTION_SPECIFIED;
            break;

        case 'f':
            ActionsSpecified += 1;
            Options |= CUT_OPTION_FIELD;
            ListString = optarg;
            break;

        case 'C':
            Options |= CUT_OPTION_COMPLEMENT;
            break;

        case 'n':
            break;

        case 's':
            Options |= CUT_OPTION_ONLY_DELIMITED |
                       CUT_OPTION_FIELD_OPTION_SPECIFIED;

            break;

        case 'D':
            OutputDelimiter = optarg;
            Options |= CUT_OPTION_FIELD_OPTION_SPECIFIED;
            break;

        case 'V':
            SwPrintVersion(CUT_VERSION_MAJOR, CUT_VERSION_MINOR);
            return 1;

        case 'h':
            printf(CUT_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    if (ActionsSpecified != 1) {
        SwPrintError(0, NULL, "Expected exactly one of -b, -c, or -f.\n");
        Status = EINVAL;
        goto MainEnd;
    }

    if (((Options & CUT_OPTION_FIELD_OPTION_SPECIFIED) != 0) &&
        ((Options & CUT_OPTION_FIELD) == 0)) {

        SwPrintError(0, NULL, "Argument only valid with -f mode.\n");
        Status = EINVAL;
        goto MainEnd;
    }

    Status = CutCreateRangeArray(ListString, &Array, &ArraySize);
    if (Status != 0) {
        goto MainEnd;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // If there were no arguments, use standard in.
    //

    if (ArgumentIndex == ArgumentCount) {
        Status = CutFile(stdin,
                         Options,
                         Delimiter,
                         OutputDelimiter,
                         Array,
                         ArraySize,
                         &LineBuffer,
                         &LineBufferSize);
    }

    //
    // Loop through the arguments again and perform the cuts.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        if (strcmp(Argument, "-") == 0) {
            Input = stdin;

        } else {
            Input = fopen(Argument, "r");
        }

        if (Input == NULL) {
            TotalStatus = errno;
            SwPrintError(Status, Argument, "Unable to open");
        }

        Status = CutFile(Input,
                         Options,
                         Delimiter,
                         OutputDelimiter,
                         Array,
                         ArraySize,
                         &LineBuffer,
                         &LineBufferSize);

        if (Input != stdin) {
            fclose(Input);
        }

        if (Status != 0) {
            TotalStatus = Status;
        }
    }

    Status = 0;

MainEnd:
    if (Array != NULL) {
        free(Array);
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
CutFile (
    FILE *Input,
    INT Options,
    CHAR Delimiter,
    PSTR OutputDelimiter,
    PCUT_RANGE Range,
    UINTN RangeSize,
    PSTR *LineBuffer,
    PUINTN LineBufferSize
    )

/*++

Routine Description:

    This routine cuts a portion of lines from the given file.

Arguments:

    Input - Supplies a pointer to the input file to read.

    Options - Supplies the CUT_OPTION_* flags governing behavior.

    Delimiter - Supplies the delimiter character for field cutting.

    OutputDelimiter - Supplies an optional pointer to the output delimiter
        string for field cutting.

    Range - Supplies a pointer to the cut range array.

    RangeSize - Supplies the size of the cut range array in elements.

    LineBuffer - Supplies a pointer to the buffer used to hold lines. This
        buffer may be allocated or reallocated by this function.

    LineBufferSize - Supplies a pointer that in input contains the size of the
        supplied line buffer. If the line buffer is reallocated, this size
        will be updated.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    LONG Element;
    PSTR FieldStart;
    BOOL FirstElement;
    BOOL LastLine;
    PSTR Line;
    CHAR OriginalSeparator;
    INT Status;

    LastLine = FALSE;
    Status = 0;
    while (LastLine == FALSE) {
        Status = CutReadLine(Input, LineBuffer, LineBufferSize, &LastLine);
        if (Status != 0) {
            break;
        }

        Line = *LineBuffer;
        if ((LastLine != FALSE) && (*Line == '\0')) {
            break;
        }

        //
        // If byte or character mode, print every character in range.
        //

        if ((Options & (CUT_OPTION_BYTE | CUT_OPTION_CHARACTER)) != 0) {
            Element = 0;
            while (*Line != '\0') {
                if (CutIsElementInRange(Element, Options, Range, RangeSize) !=
                    FALSE) {

                    putchar(*Line);
                }

                Element += 1;
                Line += 1;
            }

        //
        // In field mode, print every field in range.
        //

        } else if ((Options & CUT_OPTION_FIELD) != 0) {
            Element = 0;
            Line = *LineBuffer;
            FirstElement = TRUE;
            while (*Line != '\0') {
                FieldStart = Line;

                //
                // Find the next field.
                //

                while ((*Line != '\0') && (*Line != Delimiter)) {
                    Line += 1;
                }

                OriginalSeparator = *Line;
                if (OriginalSeparator == '\0') {

                    //
                    // If there was never any separator, print the line unless
                    // the option suppresses it.
                    //

                    if (Element == 0) {
                        if ((Options & CUT_OPTION_ONLY_DELIMITED) == 0) {
                            fputs(FieldStart, stdout);
                        }

                        break;
                    }

                } else {
                    *Line = '\0';
                    Line += 1;
                }

                if (CutIsElementInRange(Element, Options, Range, RangeSize) !=
                    FALSE) {

                    //
                    // Spit out the delimiter for this field if there was one.
                    //

                    if (FirstElement == FALSE) {
                        if (OutputDelimiter != NULL) {
                            fputs(OutputDelimiter, stdout);

                        } else {
                            putchar(Delimiter);
                        }
                    }

                    fputs(FieldStart, stdout);
                    FirstElement = FALSE;
                }

                Element += 1;
            }
        }

        putchar('\n');
    }

    return Status;
}

INT
CutReadLine (
    FILE *Input,
    PSTR *LineBuffer,
    PUINTN LineBufferSize,
    PBOOL LastLine
    )

/*++

Routine Description:

    This routine reads a line from the input.

Arguments:

    Input - Supplies a pointer to the input file to read.

    LineBuffer - Supplies a pointer to the buffer used to hold lines. This
        buffer may be allocated or reallocated by this function.

    LineBufferSize - Supplies a pointer that in input contains the size of the
        supplied line buffer. If the line buffer is reallocated, this size
        will be updated.

    LastLine - Supplies a pointer to a boolean that will be set to TRUE if the
        line returned was the last one in the file.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT Character;
    UINTN Index;
    PSTR Line;
    UINTN LineSize;
    PSTR NewBuffer;
    UINTN NewBufferSize;
    INT Status;

    Index = 0;
    Line = *LineBuffer;
    LineSize = *LineBufferSize;
    while (TRUE) {

        //
        // If the next character and a terminator is too big for the buffer,
        // reallocate it.
        //

        if (Index + 1 >= LineSize) {
            if (LineSize == 0) {
                NewBufferSize = CUT_INITIAL_LINE_SIZE;

            } else {
                NewBufferSize = LineSize * 2;
            }

            NewBuffer = realloc(Line, NewBufferSize);
            if (NewBuffer == NULL) {
                Status = ENOMEM;
                goto ReadLineEnd;
            }

            Line = NewBuffer;
            LineSize = NewBufferSize;
        }

        Character = fgetc(Input);
        if (Character == EOF) {
            *LastLine = TRUE;
            break;
        }

        Line[Index] = Character;
        Index += 1;
        if (Character == '\n') {
            break;
        }
    }

    //
    // Remove the newline and maybe a carriage return before that.
    //

    if ((Index != 0) && (Line[Index - 1] == '\n')) {
        Index -= 1;
    }

    //
    // Add a terminator.
    //

    Line[Index] = '\0';
    Index += 1;

    assert(Index <= LineSize);

    Status = 0;

ReadLineEnd:
    *LineBuffer = Line;
    *LineBufferSize = LineSize;
    return Status;
}

BOOL
CutIsElementInRange (
    LONG Element,
    INT Options,
    PCUT_RANGE RangeArray,
    UINTN RangeArraySize
    )

/*++

Routine Description:

    This routine determines whether the given element number is in the
    specified cut range array.

Arguments:

    Element - Supplies the zero-based element number. Cut ranges have been
        converted from one-based to zero-based as they were created.

    Options - Supplies the cut options. The complement bit is checked here.

    RangeArray - Supplies a pointer where an array of cut ranges will be
        returned on success. The caller is responsible for freeing this array
        when done.

    RangeArraySize - Supplies a pointer where the number of elements in the
        returned cut array will be returned.

Return Value:

    TRUE if the element should be printed.

    FALSE if the element should be discarded.

--*/

{

    UINTN Index;
    BOOL InRange;
    PCUT_RANGE Range;

    InRange = FALSE;
    for (Index = 0; Index < RangeArraySize; Index += 1) {
        Range = &(RangeArray[Index]);

        //
        // If the range starts after the element, then neither this range nor
        // any after it will match.
        //

        if (Range->Start > Element) {
            break;
        }

        //
        // The element is greater than or equal to the start. See if it's also
        // within the end, inclusive.
        //

        if ((Range->End >= Element) || (Range->End == -1)) {
            InRange = TRUE;
            break;
        }

        //
        // The element was within the start but not in the end. Move to the
        // next element. Remember, the user can specify overlapping ranges.
        //

    }

    if ((Options & CUT_OPTION_COMPLEMENT) != 0) {
        if (InRange == FALSE) {
            InRange = TRUE;

        } else {
            InRange = FALSE;
        }
    }

    return InRange;
}

INT
CutCreateRangeArray (
    PSTR RangeString,
    PCUT_RANGE *RangeArray,
    PUINTN RangeArraySize
    )

/*++

Routine Description:

    This routine creates a sorted range array for cut.

Arguments:

    RangeString - Supplies a pointer to a string describing the range.

    RangeArray - Supplies a pointer where an array of cut ranges will be
        returned on success. The caller is responsible for freeing this array
        when done.

    RangeArraySize - Supplies a pointer where the number of elements in the
        returned cut array will be returned.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PCUT_RANGE Array;
    UINTN ArraySize;
    PSTR CurrentString;
    LONG End;
    UINTN MaxCount;
    LONG Start;
    INT Status;

    Array = NULL;
    ArraySize = 0;

    //
    // Loop once counting commas and blanks to figure out the maximum number of
    // elements.
    //

    MaxCount = 1;
    CurrentString = RangeString;
    while (*CurrentString != '\0') {
        if ((*CurrentString == ',') || (isblank(*CurrentString) != 0)) {
            MaxCount += 1;
        }

        CurrentString += 1;
    }

    Array = malloc(MaxCount * sizeof(CUT_RANGE));
    if (Array == NULL) {
        return ENOMEM;
    }

    Status = EINVAL;

    //
    // Loop through the string and create the ranges.
    //

    CurrentString = RangeString;
    while (*CurrentString != '\0') {
        End = 0;
        Start = 0;

        //
        // If it starts with a digit, it could be N, N-, or N-M.
        //

        if (isdigit(*CurrentString) != 0) {
            Start = strtoul(CurrentString, &AfterScan, 10);
            if (AfterScan == CurrentString) {
                SwPrintError(0, NULL, "Invalid byte/field list");
                goto CreateRangeArrayEnd;
            }

            CurrentString = AfterScan;
            End = Start;

            //
            // Accept a - for N- and N-M.
            //

            if (*CurrentString == '-') {
                CurrentString += 1;
                End = -1;
                if (isdigit(*CurrentString) != 0) {
                    End = strtoul(CurrentString, &AfterScan, 10);
                    if (AfterScan == CurrentString) {
                        SwPrintError(0, NULL, "Invalid byte/field list");
                        goto CreateRangeArrayEnd;
                    }

                    CurrentString = AfterScan;
                }
            }

        //
        // If it starts with a - it must be -M.
        //

        } else if (*CurrentString == '-') {
            CurrentString += 1;
            Start = 1;
            End = strtoul(CurrentString, &AfterScan, 10);
            if (AfterScan == CurrentString) {
                SwPrintError(0, NULL, "Invalid byte/field list");
                goto CreateRangeArrayEnd;
            }

            CurrentString = AfterScan;
        }

        if ((Start == 0) || (End == 0)) {
            SwPrintError(0, NULL, "Byte/field lists start at 1");
            goto CreateRangeArrayEnd;
        }

        if ((End != -1) && (End < Start)) {
            SwPrintError(0,
                         NULL,
                         "Byte/field range should be in ascending order");

            goto CreateRangeArrayEnd;
        }

        Start -= 1;
        if (End != -1) {
            End -= 1;
        }

        //
        // The end of the string or a separator should be here.
        //

        if ((isspace(*CurrentString) == 0) && (*CurrentString != ',') &&
            (*CurrentString != '\0')) {

            SwPrintError(0, CurrentString, "Expected separator");
            goto CreateRangeArrayEnd;
        }

        if (*CurrentString != '\0') {
            CurrentString += 1;
            if (*CurrentString == '\0') {
                SwPrintError(0, NULL, "Range expected after separator");
                goto CreateRangeArrayEnd;
            }
        }

        //
        // Insert the range.
        //

        assert(ArraySize < MaxCount);

        Array[ArraySize].Start = Start;
        Array[ArraySize].End = End;
        ArraySize += 1;
    }

    if (ArraySize == 0) {
        SwPrintError(0, NULL, "Byte/field list expected");
        goto CreateRangeArrayEnd;
    }

    //
    // Sort the range array.
    //

    qsort(Array, ArraySize, sizeof(CUT_RANGE), CutCompareCutRanges);
    Status = 0;

CreateRangeArrayEnd:
    if (Status != 0) {
        if (Array != NULL) {
            free(Array);
            Array = NULL;
            ArraySize = 0;
        }
    }

    *RangeArray = Array;
    *RangeArraySize = ArraySize;
    return Status;
}

int
CutCompareCutRanges (
    const void *First,
    const void *Second
    )

/*++

Routine Description:

    This routine compares two cut range structures by their start offset. This
    is used by the quicksort routine.

Arguments:

    First - Supplies the first range structure to compare.

    Second - Supplies the second range structure to compare.

Return Value:

    -1 if FirstRange < SecondRange.

    0 if FirstRange == SecondRange.

    1 if FirstRange > SecondRange.

--*/

{

    PCUT_RANGE FirstRange;
    PCUT_RANGE SecondRange;

    FirstRange = (PCUT_RANGE)First;
    SecondRange = (PCUT_RANGE)Second;
    if (FirstRange->Start < SecondRange->Start) {
        return -1;

    } else if (FirstRange->Start > SecondRange->Start) {
        return 1;
    }

    return 0;
}


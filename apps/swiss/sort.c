/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sort.c

Abstract:

    This module implements the sort utility.

Author:

    Evan Green 15-Aug-2013

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

#define SORT_VERSION_MAJOR 1
#define SORT_VERSION_MINOR 0

#define SORT_USAGE                                                             \
    "usage: sort [-m][-o output][-bdfinru][-t char][-k keydef]... [file...]\n" \
    "       sort -c [-bdfinru][-t char][-k keydef][file]\n\n"                  \
    "The sort utility either sorts all lines in a file, merges line of all \n" \
    "the named (presorted) files together, or checks to see if a single \n"    \
    "input file is sorted.\n"                                                  \
    "Options are:\n"                                                           \
    "  -c, --check -- Check that a single input file is sorted. No output \n"  \
    "        shall be produced, only the exit code is affected.\n"             \
    "  -m, --merge -- Merge only. The input files are assumed to be sorted.\n" \
    "  -o, --output <file> -- Specify an output file to be used instead of \n" \
    "        standard out. This file can be the same as one of the input "     \
    "files.\n"                                                                 \
    "  -u, --unique -- Unique: suppress all but one in each set of lines \n"   \
    "        having equal keys. If used with the -c option, check that  \n"    \
    "        there are no lines with duplicate keys, in addition to \n"        \
    "        checking that the input file is sorted.\n"                        \
    "  -d, --dictionary-order -- Only blanks and alphanumeric characters \n"   \
    "        shall be significant in comparisons.\n"                           \
    "  -f, --ignore-case -- Convert any lower case characters to uppercase \n" \
    "        when comparing.\n"                                                \
    "  -i, --ignore-nonprinting -- Ignore non-printable characters.\n"         \
    "  -n, --numeric-sort -- Sort numerically.\n"                              \
    "  -r, --reverse -- Reverse the sort order.\n"                             \
    "  -b, --ignore-leading-blanks -- Ignore leading blanks.\n"                \
    "  -k, --key <keydef> -- Restrict the sorting key to a certain region \n"  \
    "        of the line. The keydef parameter is defined by:\n"               \
    "        field_start[type][,field_end[type]]\n"                            \
    "        where field_start takes the form \n"                              \
    "        field_number[.first_character] and field_end takes the form \n"   \
    "        field_number[.last_character]. \n"                                \
    "        Fields and characters are indexed from 1. The type parameter \n"  \
    "        is one or more of [bdfinr], which attach the corresponding \n"    \
    "        flag meaning to that specific field.\n"                           \
    "  -t, --field-separator <character> -- Use the given character as a \n"   \
    "        field separator.\n"                                               \
    "  file -- Supplies the input file to sort. If no file is supplied or \n"  \
    "        the file is -, then use stdin.\n\n"

#define SORT_OPTIONS_STRING "cmo:udfinrbk:t:"

//
// Set this option to ignore leading blanks in comparisons.
//

#define SORT_OPTION_IGNORE_LEADING_BLANKS 0x00000001

//
// Set this option to consider only blanks and alphanumeric characters in
// comparisons.
//

#define SORT_OPTION_ONLY_ALPHANUMERICS 0x00000002

//
// Set this option to uppercase everything for comparisons.
//

#define SORT_OPTION_UPPERCASE_EVERYTHING 0x00000004

//
// Set this option to ignore non-printable characters during the comparison.
//

#define SORT_OPTION_IGNORE_NONPRINTABLE 0x00000008

//
// Set this option to convert strings to integers and compare those.
//

#define SORT_OPTION_COMPARE_NUMERICALLY 0x00000010

//
// Set this option to reverse the sort order.
//

#define SORT_OPTION_REVERSE 0x00000020

//
// Set this option to check if a file is sorted.
//

#define SORT_OPTION_CHECK_ONLY 0x00000040

//
// Set this option to merge only.
//

#define SORT_OPTION_MERGE_ONLY 0x00000080

//
// Set this option to only print unique lines, and check for uniqueness on
// checks.
//

#define SORT_OPTION_UNIQUE 0x00000100

#define SORT_INITIAL_ELEMENT_COUNT 32
#define SORT_INITIAL_STRING_SIZE 32

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
VOID
(*PSORT_DESTROY_ARRAY_ELEMENT_ROUTINE) (
    PVOID Element
    );

/*++

Routine Description:

    This routine is called to destroy an element in a sort array.

Arguments:

    Element - Supplies the element being destroyed.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a mutable array of pointers in the sort utility.

Members:

    Data - Supplies a pointer to the malloced string array buffer itself.

    Size - Supplies the number of valid elements in the buffer.

    Capacity - Supplies the maximum number of elements before the array needs
        to be reallocated.

--*/

typedef struct _SORT_ARRAY {
    PVOID *Data;
    UINTN Size;
    UINTN Capacity;
} SORT_ARRAY, *PSORT_ARRAY;

/*++

Structure Description:

    This structure defines a mutable string in the sort utility.

Members:

    Data - Supplies a pointer to the malloced string buffer itself.

    Size - Supplies the number of valid bytes in the buffer including the
        null terminator if there is one.

    Capacity - Supplies the size of the buffer allocation.

--*/

typedef struct _SORT_STRING {
    PSTR Data;
    UINTN Size;
    UINTN Capacity;
} SORT_STRING, *PSORT_STRING;

/*++

Structure Description:

    This structure defines an input file to the sort utility.

Members:

    File - Stores the open file pointer, or NULL if the file could not be
        opened.

    Line - Stores a pointer to the string containing the most recent line.

--*/

typedef struct _SORT_INPUT {
    FILE *File;
    PSORT_STRING Line;
} SORT_INPUT, *PSORT_INPUT;

/*++

Structure Description:

    This structure defines a sort key used by the sort utility.

Members:

    StartField - Stores the starting field number to use in comparisons.

    StartCharacter - Stores the starting character within the starting field
        to use in comparisons.

    StartOptions - Stores any specific options attached to the start field.

    EndField - Stores the ending field, or -1 for the end of the line.

    EndCharacter - Stores the ending character within the ending field, or -1
        for the end of the field.

    EndOptions - Stores options associated with the ending field.

--*/

typedef struct _SORT_KEY {
    LONG StartField;
    LONG StartCharacter;
    ULONG StartOptions;
    LONG EndField;
    LONG EndCharacter;
    ULONG EndOptions;
} SORT_KEY, *PSORT_KEY;

/*++

Structure Description:

    This structure defines the context for an instantiation of the sort utility.

Members:

    Input - Stores the array of pointers to sort inputs.

    Key - Stores the array of pointers to sort keys.

    KeyCount - Stores the number of elements in the key array.

    Options - Stores the global options.

    Output - Stores the name of the output file.

    Separator - Stores the field separator character, or -1 if none was
        supplied.

--*/

typedef struct _SORT_CONTEXT {
    SORT_ARRAY Input;
    SORT_ARRAY Key;
    ULONG Options;
    PSTR Output;
    INT Separator;
} SORT_CONTEXT, *PSORT_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SortCheckFile (
    PSORT_CONTEXT Context,
    PSORT_INPUT Input
    );

INT
SortMergeSortedFiles (
    PSORT_CONTEXT Context,
    FILE *Output
    );

INT
SortCompareLines (
    const VOID *LeftPointer,
    const VOID *RightPointer
    );

INT
SortReadLine (
    PSORT_CONTEXT Context,
    PSORT_INPUT Input,
    PSORT_STRING Holding,
    PSORT_STRING *String
    );

INT
SortAddInputFile (
    PSORT_CONTEXT Context,
    PSTR FileName
    );

INT
SortAddKey (
    PSORT_CONTEXT Context,
    PSTR Argument
    );

VOID
SortScanKeyFlags (
    PSTR *Argument,
    PULONG Flags
    );

INT
SortArrayAddElement (
    PSORT_ARRAY Array,
    PVOID Element
    );

INT
SortStringAddCharacter (
    PSORT_STRING String,
    CHAR Character
    );

PSORT_STRING
SortCreateString (
    PSTR InitialData,
    UINTN InitialDataSize
    );

VOID
SortDestroyString (
    PSORT_STRING String
    );

VOID
SortDestroyArray (
    PSORT_ARRAY Array,
    PSORT_DESTROY_ARRAY_ELEMENT_ROUTINE DestroyElementRoutine
    );

VOID
SortDestroyInput (
    PSORT_INPUT Input
    );

VOID
SortGetFieldOffset (
    PSORT_STRING String,
    INT Separator,
    LONG Field,
    LONG Character,
    PULONG Offset
    );

LONG
SortStringToLong (
    PSORT_STRING String,
    ULONG Options,
    ULONG Offset
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a global pointer to the sort context since a context pointer can't be
// passed through the qsort routine to the compare function.
//

PSORT_CONTEXT SortContext;

struct option SortLongOptions[] = {
    {"check", no_argument, 0, 'c'},
    {"merge", no_argument, 0, 'm'},
    {"output", required_argument, 0, 'o'},
    {"unique", no_argument, 0, 'u'},
    {"dictionary-order", no_argument, 0, 'd'},
    {"ignore-case", no_argument, 0, 'f'},
    {"ignore-nonprinting", no_argument, 0, 'i'},
    {"numeric-sort", no_argument, 0, 'n'},
    {"reverse", no_argument, 0, 'r'},
    {"ignore-leading-blanks", no_argument, 0, 'b'},
    {"key", required_argument, 0, 'k'},
    {"field-separator", required_argument, 0, 't'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
SortMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the sort utility.

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
    SORT_CONTEXT Context;
    PSORT_INPUT Input;
    ULONG InputIndex;
    PSORT_STRING InputLine;
    SORT_ARRAY InputLines;
    SORT_STRING InputString;
    PSORT_KEY Key;
    UINTN KeyIndex;
    UINTN LineIndex;
    INT Option;
    FILE *Output;
    PSORT_STRING PreviousLine;
    INT Status;

    Input = NULL;
    InputLine = NULL;
    memset(&Context, 0, sizeof(SORT_CONTEXT));
    memset(&InputString, 0, sizeof(SORT_STRING));
    memset(&InputLines, 0, sizeof(SORT_ARRAY));
    Context.Separator = -1;
    Output = NULL;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SORT_OPTIONS_STRING,
                             SortLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            return Status;
        }

        switch (Option) {
        case 'c':
            Context.Options |= SORT_OPTION_CHECK_ONLY;
            break;

        case 'm':
            Context.Options |= SORT_OPTION_MERGE_ONLY;
            break;

        case 'o':
            Context.Output = optarg;

            assert(Context.Output != NULL);

            break;

        case 'u':
            Context.Options |= SORT_OPTION_UNIQUE;
            break;

        case 'd':
            Context.Options |= SORT_OPTION_ONLY_ALPHANUMERICS;
            break;

        case 'f':
            Context.Options |= SORT_OPTION_UPPERCASE_EVERYTHING;
            break;

        case 'i':
            Context.Options |= SORT_OPTION_IGNORE_NONPRINTABLE;
            break;

        case 'n':
            Context.Options |= SORT_OPTION_COMPARE_NUMERICALLY;
            break;

        case 'r':
            Context.Options |= SORT_OPTION_REVERSE;
            break;

        case 'b':
            Context.Options |= SORT_OPTION_IGNORE_LEADING_BLANKS;
            break;

        case 'k':
            Argument = optarg;

            assert(Argument != NULL);

            Status = SortAddKey(&Context, Argument);
            if (Status != 0) {
                SwPrintError(0, Argument, "Invalid key argument");
                return Status;
            }

            break;

        case 't':
            Argument = optarg;

            assert(Argument != NULL);

            if (strlen(Argument) == 1) {
                Context.Separator = *Argument;

            } else if ((*Argument == '\\') && (strlen(Argument) == 2)) {
                switch (*(Argument + 1)) {
                case 'a':
                    Context.Separator = '\a';
                    break;

                case 'b':
                    Context.Separator = '\b';
                    break;

                case 'f':
                    Context.Separator = '\f';
                    break;

                case 'n':
                    Context.Separator = '\n';
                    break;

                case 'r':
                    Context.Separator = '\r';
                    break;

                case 't':
                    Context.Separator = '\t';
                    break;

                case 'v':
                    Context.Separator = '\v';
                    break;

                case '0':
                    Context.Separator = '\0';
                    break;

                case '\\':
                    Context.Separator = '\\';
                    break;

                default:
                    SwPrintError(0,
                                 Argument,
                                 "Field separator should be a single "
                                 "character");

                    return 2;
                }

            } else {
                SwPrintError(0,
                             Argument,
                             "Field separator should be a single "
                             "character");

                return 2;
            }

            break;

        case 'V':
            SwPrintVersion(SORT_VERSION_MAJOR, SORT_VERSION_MINOR);
            return 1;

        case 'h':
            printf(SORT_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            return Status;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Add the remainder of the arguments as inputs.
    //

    while (ArgumentIndex < ArgumentCount) {
        Status = SortAddInputFile(&Context, Arguments[ArgumentIndex]);
        if (Status != 0) {
            goto MainEnd;
        }

        ArgumentIndex += 1;
    }

    //
    // Use standard out if nothing else was supplied.
    //

    if (Context.Input.Size == 0) {
        Status = SortAddInputFile(&Context, "-");
        if (Status != 0) {
            goto MainEnd;
        }
    }

    //
    // If there are no keys, create a default one of the whole line.
    //

    if (Context.Key.Size == 0) {
        Status = SortAddKey(&Context, "1");
        if (Status != 0) {
            goto MainEnd;
        }
    }

    //
    // Copy in the global flags to the key flags to avoid extra work during
    // compares.
    //

    for (KeyIndex = 0; KeyIndex < Context.Key.Size; KeyIndex += 1) {
        Key = Context.Key.Data[KeyIndex];
        Key->StartOptions |= Context.Options;
        Key->EndOptions |= Context.Options;
    }

    //
    // Open up the output if needed.
    //

    if (Context.Output != NULL) {
        Output = fopen(Context.Output, "w");
        if (Output == NULL) {
            Status = errno;
            SwPrintError(Status, Context.Output, "Failed to open output");
            goto MainEnd;
        }

    } else {
        Output = stdout;
    }

    //
    // All the arguments are parsed, start the work.
    //

    SortContext = &Context;
    if ((Context.Options & SORT_OPTION_CHECK_ONLY) != 0) {
        if (Context.Input.Size != 1) {
            SwPrintError(0, NULL, "Only one file can be specified with -c");
            Status = 2;
            goto MainEnd;
        }

        Status = SortCheckFile(&Context, Context.Input.Data[0]);
        goto MainEnd;

    } else if ((Context.Options & SORT_OPTION_MERGE_ONLY) != 0) {
        Status = SortMergeSortedFiles(&Context, Output);
        goto MainEnd;
    }

    //
    // This is the real sort, not merge or check. Read in all inputs.
    //

    for (InputIndex = 0; InputIndex < Context.Input.Size; InputIndex += 1) {
        Input = Context.Input.Data[InputIndex];
        while (TRUE) {
            Status = SortReadLine(&Context,
                                  Input,
                                  &InputString,
                                  &InputLine);

            if (Status != 0) {
                SwPrintError(Status, NULL, "Failed to read line");
                goto MainEnd;
            }

            if (InputLine == NULL) {
                break;
            }

            Status = SortArrayAddElement(&InputLines, InputLine);
            if (Status != 0) {
                SortDestroyString(InputLine);
                goto MainEnd;
            }

            InputLine = NULL;
        }
    }

    if (InputLines.Size == 0) {
        Status = 0;
        goto MainEnd;
    }

    //
    // Do it, sort the arrays.
    //

    qsort(InputLines.Data, InputLines.Size, sizeof(PVOID), SortCompareLines);

    //
    // Write all the lines to the output.
    //

    PreviousLine = NULL;
    for (LineIndex = 0; LineIndex < InputLines.Size; LineIndex += 1) {
        InputLine = InputLines.Data[LineIndex];

        //
        // If the unique flag is off, this is the first line, or the lines
        // aren't equal then print the line.
        //

        if (((Context.Options & SORT_OPTION_UNIQUE) == 0) ||
            (PreviousLine == NULL) ||
            (SortCompareLines(&PreviousLine, &InputLine) != 0)) {

            fprintf(Output, "%s\n", InputLine->Data);
        }

        PreviousLine = InputLine;
    }

    Status = 0;

MainEnd:
    SortContext = NULL;
    if ((Output != NULL) && (Output != stdout)) {
        fclose(Output);
    }

    SortDestroyArray(&(Context.Input),
                     (PSORT_DESTROY_ARRAY_ELEMENT_ROUTINE)SortDestroyInput);

    SortDestroyArray(&(Context.Key), free);
    if (InputString.Data != NULL) {
        free(InputString.Data);
    }

    SortDestroyArray(&InputLines,
                     (PSORT_DESTROY_ARRAY_ELEMENT_ROUTINE)SortDestroyString);

    if ((Status != 0) && (Status != 1)) {
        SwPrintError(Status, NULL, "Sort exiting abnormally");
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SortCheckFile (
    PSORT_CONTEXT Context,
    PSORT_INPUT Input
    )

/*++

Routine Description:

    This routine checks a file to see if it is sorted (and optionally unique).

Arguments:

    Context - Supplies a pointer to the application context.

    Input - Supplies a pointer to the input to check.

Return Value:

    0 if the file is sorted.

    1 if the file is not sorted.

    >1 error number if an error occurred.

--*/

{

    INT Comparison;
    PSORT_STRING Line;
    PSORT_STRING PreviousLine;
    INT Status;
    SORT_STRING WorkingBuffer;

    memset(&WorkingBuffer, 0, sizeof(SORT_STRING));
    Line = NULL;
    PreviousLine = NULL;
    while (TRUE) {
        Status = SortReadLine(Context,
                              Input,
                              &WorkingBuffer,
                              &Line);

        if (Status != 0) {
            SwPrintError(Status, NULL, "Failed to read file");
            goto CheckFileEnd;
        }

        if (Line == NULL) {
            break;
        }

        if (PreviousLine != NULL) {
            Comparison = SortCompareLines(&PreviousLine, &Line);
            if (Comparison > 1) {
                Status = 1;
                goto CheckFileEnd;
            }

            if (((Context->Options & SORT_OPTION_UNIQUE) != 0) &&
                (Comparison == 0)) {

                Status = 1;
                goto CheckFileEnd;
            }

            SortDestroyString(PreviousLine);
        }

        PreviousLine = Line;
        Line = NULL;
    }

    Status = 0;

CheckFileEnd:
    if (WorkingBuffer.Data != NULL) {
        free(WorkingBuffer.Data);
    }

    if (Line != NULL) {
        SortDestroyString(Line);
    }

    if (PreviousLine != NULL) {
        SortDestroyString(PreviousLine);
    }

    return Status;
}

INT
SortMergeSortedFiles (
    PSORT_CONTEXT Context,
    FILE *Output
    )

/*++

Routine Description:

    This routine merges several files that are already in order.

Arguments:

    Context - Supplies a pointer to the application context.

    Output - Supplies a pointer to the output file to write to.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Comparison;
    PSORT_INPUT Input;
    UINTN InputIndex;
    PSORT_STRING PreviousWinner;
    INT Status;
    PSORT_INPUT Winner;
    SORT_STRING WorkingBuffer;

    PreviousWinner = NULL;
    memset(&WorkingBuffer, 0, sizeof(SORT_STRING));

    //
    // Prime all the inputs by reading their first lines.
    //

    for (InputIndex = 0; InputIndex < Context->Input.Size; InputIndex += 1) {
        Input = Context->Input.Data[InputIndex];
        Status = SortReadLine(Context,
                              Input,
                              &WorkingBuffer,
                              &(Input->Line));

        if (Status != 0) {
            SwPrintError(Status, NULL, "Failed to read file");
            goto MergeSortedFilesEnd;
        }
    }

    //
    // Loop getting the winning line until all files are drained.
    //

    while (TRUE) {
        Winner = NULL;
        for (InputIndex = 0;
             InputIndex < Context->Input.Size;
             InputIndex += 1) {

            Input = Context->Input.Data[InputIndex];
            if (Input->Line == NULL) {
                continue;
            }

            if (Winner == NULL) {
                Winner = Input;

            } else {

                //
                // If this input's line is less than the current winner, set it
                // to the new winner.
                //

                Comparison = SortCompareLines(&(Input->Line), &(Winner->Line));
                if (Comparison < 0) {
                    Winner = Input;
                }
            }
        }

        //
        // If there is no winner, then all files must be drained. Stop.
        //

        if (Winner == NULL) {
            break;
        }

        //
        // Print the line.
        //

        if (((Context->Options & SORT_OPTION_UNIQUE) == 0) ||
            (PreviousWinner == NULL) ||
            (SortCompareLines(&(Winner->Line), &PreviousWinner) != 0)) {

            fprintf(Output, "%s\n", Winner->Line->Data);
        }

        //
        // Set the new previous winner, and read a new line from that winning
        // file.
        //

        if (PreviousWinner != NULL) {
            SortDestroyString(PreviousWinner);
        }

        PreviousWinner = Winner->Line;
        if (Winner->Line != NULL) {
            Status = SortReadLine(Context,
                                  Winner,
                                  &WorkingBuffer,
                                  &(Winner->Line));

            if (Status != 0) {
                SwPrintError(Status, NULL, "Failed to read file");
                goto MergeSortedFilesEnd;
            }
        }
    }

    Status = 0;

MergeSortedFilesEnd:
    if (PreviousWinner != NULL) {
        SortDestroyString(PreviousWinner);
    }

    if (WorkingBuffer.Data != NULL) {
        free(WorkingBuffer.Data);
    }

    return Status;
}

INT
SortCompareLines (
    const VOID *LeftPointer,
    const VOID *RightPointer
    )

/*++

Routine Description:

    This routine compares two sort string elements.

Arguments:

    LeftPointer - Supplies a pointer containing a pointer to the left string
        of the comparison.

    RightPointer - Supplies a pointer containing a pointer to the right string
        of the comparison.

Return Value:

    1 if Left > Right.

    0 if Left == Right.

    -1 if Left < Right.

--*/

{

    PSORT_CONTEXT Context;
    PSORT_KEY Key;
    UINTN KeyIndex;
    PSORT_STRING Left;
    CHAR LeftCharacter;
    ULONG LeftEndIndex;
    ULONG LeftStartIndex;
    LONG LeftValue;
    ULONG Options;
    INT Result;
    PSORT_STRING Right;
    CHAR RightCharacter;
    ULONG RightEndIndex;
    ULONG RightStartIndex;
    LONG RightValue;

    Context = SortContext;
    Left = *(PSORT_STRING *)LeftPointer;
    Right = *(PSORT_STRING *)RightPointer;
    for (KeyIndex = 0; KeyIndex < Context->Key.Size; KeyIndex += 1) {
        Key = Context->Key.Data[KeyIndex];
        Options = Key->StartOptions | Key->EndOptions;
        SortGetFieldOffset(Left,
                           Context->Separator,
                           Key->StartField,
                           Key->StartCharacter,
                           &LeftStartIndex);

        SortGetFieldOffset(Left,
                           Context->Separator,
                           Key->EndField,
                           Key->EndCharacter,
                           &LeftEndIndex);

        SortGetFieldOffset(Right,
                           Context->Separator,
                           Key->StartField,
                           Key->StartCharacter,
                           &RightStartIndex);

        SortGetFieldOffset(Right,
                           Context->Separator,
                           Key->EndField,
                           Key->EndCharacter,
                           &RightEndIndex);

        //
        // Strip leading blanks if requested.
        //

        if ((Options & SORT_OPTION_IGNORE_LEADING_BLANKS) != 0) {
            while ((LeftStartIndex < LeftEndIndex) &&
                   (isblank(Left->Data[LeftStartIndex]))) {

                LeftStartIndex += 1;
            }

            while ((RightStartIndex < RightEndIndex) &&
                   (isblank(Right->Data[RightStartIndex]))) {

                RightStartIndex += 1;
            }
        }

        //
        // Scan and compare the numbers if sorting numerically.
        //

        if ((Options & SORT_OPTION_COMPARE_NUMERICALLY) != 0) {
            LeftValue = SortStringToLong(Left, Options, LeftStartIndex);
            RightValue = SortStringToLong(Right, Options, RightStartIndex);
            if (LeftValue < RightValue) {
                Result = -1;
                if ((Options & SORT_OPTION_REVERSE) != 0) {
                    Result = -Result;
                }

                goto CompareLinesEnd;

            } else if (LeftValue > RightValue) {
                Result = 1;
                if ((Options & SORT_OPTION_REVERSE) != 0) {
                    Result = -Result;
                }

                goto CompareLinesEnd;
            }

        //
        // Compare alphabetically.
        //

        } else {
            while ((LeftStartIndex < LeftEndIndex) ||
                   (RightStartIndex < RightEndIndex)) {

                //
                // Get the left and right characters. Skip non-printables or
                // non-alphanumeric and non-blank if requested.
                //

                LeftCharacter = 0;
                RightCharacter = 0;
                if (LeftStartIndex < LeftEndIndex) {
                    LeftCharacter = Left->Data[LeftStartIndex];
                    if (((Options & SORT_OPTION_IGNORE_NONPRINTABLE) != 0) &&
                        (!isprint(LeftCharacter))) {

                        LeftStartIndex += 1;
                        continue;
                    }

                    if (((Options & SORT_OPTION_ONLY_ALPHANUMERICS) != 0) &&
                        (!isalnum(LeftCharacter)) &&
                        (!isspace(LeftCharacter))) {

                        LeftStartIndex += 1;
                        continue;
                    }
                }

                if (RightStartIndex < RightEndIndex) {
                    RightCharacter = Right->Data[RightStartIndex];
                    if (((Options & SORT_OPTION_IGNORE_NONPRINTABLE) != 0) &&
                        (!isprint(RightCharacter))) {

                        RightStartIndex += 1;
                        continue;
                    }

                    if (((Options & SORT_OPTION_ONLY_ALPHANUMERICS) != 0) &&
                        (!isalnum(RightCharacter)) &&
                        (!isspace(RightCharacter))) {

                        RightStartIndex += 1;
                        continue;
                    }

                }

                //
                // Upper-case the characters if requested.
                //

                if ((Options & SORT_OPTION_UPPERCASE_EVERYTHING) != 0) {
                    LeftCharacter = toupper(LeftCharacter);
                    RightCharacter = toupper(RightCharacter);
                }

                //
                // Finally finally finally compare the values.
                //

                if (LeftCharacter < RightCharacter) {
                    Result = -1;
                    if ((Options & SORT_OPTION_REVERSE) != 0) {
                        Result = -Result;
                    }

                    goto CompareLinesEnd;

                } else if (LeftCharacter > RightCharacter) {
                    Result = 1;
                    if ((Options & SORT_OPTION_REVERSE) != 0) {
                        Result = -Result;
                    }

                    goto CompareLinesEnd;
                }

                LeftStartIndex += 1;
                RightStartIndex += 1;
            }
        }
    }

    //
    // After all that, they come out the same.
    //

    Result = 0;

CompareLinesEnd:
    return Result;
}

INT
SortReadLine (
    PSORT_CONTEXT Context,
    PSORT_INPUT Input,
    PSORT_STRING Holding,
    PSORT_STRING *String
    )

/*++

Routine Description:

    This routine creates a string containing the next input line.

Arguments:

    Context - Supplies a pointer to the application context.

    Input - Supplies a pointer to the input.

    Holding - Supplies a pointer to a transitory buffer to use to hold the
        string while it's being read.

    String - Supplies a pointer where the string will be returned on success.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    INT Character;
    PSORT_STRING NewString;
    INT Result;

    Holding->Size = 0;
    NewString = NULL;
    Character = fgetc(Input->File);
    if (Character == EOF) {
        Result = 0;
        goto ReadLineEnd;
    }

    //
    // Loop reading characters and shoving them into the holding buffer.
    //

    while (TRUE) {
        if (Character == EOF) {
            break;

        } else if (Character == '\n') {

            //
            // Peel off a carriage return too if it's there.
            //

            if ((Holding->Size != 0) &&
                (Holding->Data[Holding->Size - 1] == '\r')) {

                Holding->Size -= 1;
            }

            break;
        }

        Result = SortStringAddCharacter(Holding, (CHAR)Character);
        if (Result != 0) {
            goto ReadLineEnd;
        }

        Character = fgetc(Input->File);
    }

    //
    // Terminate the string.
    //

    Result = SortStringAddCharacter(Holding, '\0');
    if (Result != 0) {
        goto ReadLineEnd;
    }

    //
    // Create a new string that's well sized.
    //

    NewString = SortCreateString(Holding->Data, Holding->Size);
    if (NewString == NULL) {
        Result = ENOMEM;
        goto ReadLineEnd;
    }

    Result = 0;

ReadLineEnd:
    *String = NewString;
    return Result;
}

INT
SortAddInputFile (
    PSORT_CONTEXT Context,
    PSTR FileName
    )

/*++

Routine Description:

    This routine adds an input file to the sort input array.

Arguments:

    Context - Supplies a pointer to the application context.

    FileName - Supplies a pointer to the file to add.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSORT_INPUT Input;
    INT Status;

    Input = malloc(sizeof(SORT_INPUT));
    if (Input == NULL) {
        Status = ENOMEM;
        goto AddInputFileEnd;
    }

    memset(Input, 0, sizeof(SORT_INPUT));
    if (strcmp(FileName, "-") == 0) {
        Input->File = stdin;

    } else {
        Input->File = fopen(FileName, "rb");
        if (Input->File == NULL) {
            Status = errno;
            goto AddInputFileEnd;
        }
    }

    Status = SortArrayAddElement(&(Context->Input), Input);
    if (Status != 0) {
        goto AddInputFileEnd;
    }

    Status = 0;
    Input = NULL;

AddInputFileEnd:
    if (Input != NULL) {
        if (Input->File != NULL) {
            fclose(Input->File);
        }

        free(Input);
    }

    return Status;
}

INT
SortAddKey (
    PSORT_CONTEXT Context,
    PSTR Argument
    )

/*++

Routine Description:

    This routine adds a sort key to the application context.

Arguments:

    Context - Supplies a pointer to the application context.

    Argument - Supplies a pointer to the key argument to parse and add.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PSORT_KEY Key;
    INT Status;
    LONG Value;

    Key = malloc(sizeof(SORT_KEY));
    if (Key == NULL) {
        Status = ENOMEM;
        SwPrintError(0, NULL, "Allocation failure");
        goto AddKeyEnd;
    }

    memset(Key, 0, sizeof(SORT_KEY));
    Key->StartField = 1;
    Key->StartCharacter = 1;
    Key->EndField = -1;
    Key->EndCharacter = -1;

    //
    // The total format looks like field_start[type][,field_end[type]], where
    // type is [bdfinr] repeated, and field start and end are int[.int]. Start
    // with the field_start field number.
    //

    Status = EINVAL;
    Value = strtol(Argument, &AfterScan, 10);
    if (Value <= 0) {
        SwPrintError(0, Argument, "Invalid key start field");
        goto AddKeyEnd;
    }

    Key->StartField = Value;

    //
    // If there's a dot next, scan the starting character.
    //

    Argument = AfterScan;
    if (*Argument == '.') {
        Argument += 1;
        Value = strtol(Argument, &AfterScan, 10);
        if (Value <= 0) {
            SwPrintError(0, Argument, "Invalid key start character");
            goto AddKeyEnd;
        }

        Key->StartCharacter = Value;
        Argument = AfterScan;
    }

    //
    // Scan any flags.
    //

    SortScanKeyFlags(&Argument, &(Key->StartOptions));

    //
    // If there's a comma, scan the end field number.
    //

    if (*Argument == ',') {
        Argument += 1;
        Value = strtol(Argument, &AfterScan, 10);
        if (Value <= 0) {
            SwPrintError(0, Argument, "Invalid key end field");
            goto AddKeyEnd;
        }

        Key->EndField = Value;

        //
        // If there's a dot next, scan the ending character.
        //

        Argument = AfterScan;
        if (*Argument == '.') {
            Argument += 1;
            Value = strtol(Argument, &AfterScan, 10);
            if (Value <= 0) {
                SwPrintError(0, Argument, "Invalid key end character");
                goto AddKeyEnd;
            }

            Key->EndCharacter = Value;
            Argument = AfterScan;
        }

        //
        // Scan any flags.
        //

        SortScanKeyFlags(&Argument, &(Key->StartOptions));
    }

    //
    // Add this new key to the array.
    //

    Status = SortArrayAddElement(&(Context->Key), Key);
    if (Status != 0) {
        goto AddKeyEnd;
    }

    Status = 0;
    Key = NULL;

AddKeyEnd:
    if (Key != NULL) {
        free(Key);
    }

    return Status;
}

VOID
SortScanKeyFlags (
    PSTR *Argument,
    PULONG Flags
    )

/*++

Routine Description:

    This routine scans sort key type flags.

Arguments:

    Argument - Supplies a pointer that on input contains a pointer where the
        flags might be. On output, this pointer is advanced past any flags.

    Flags - Supplies a pointer where the flags will be ORed in.

Return Value:

    None.

--*/

{

    PSTR String;

    String = *Argument;
    while (*String != '\0') {
        switch (*String) {
        case 'b':
            *Flags |= SORT_OPTION_IGNORE_LEADING_BLANKS;
            break;

        case 'd':
            *Flags |= SORT_OPTION_ONLY_ALPHANUMERICS;
            break;

        case 'f':
            *Flags |= SORT_OPTION_UPPERCASE_EVERYTHING;
            break;

        case 'i':
            *Flags |= SORT_OPTION_IGNORE_NONPRINTABLE;
            break;

        case 'n':
            *Flags |= SORT_OPTION_COMPARE_NUMERICALLY;
            break;

        case 'r':
            *Flags |= SORT_OPTION_REVERSE;
            break;

        default:
            goto ScanKeyFlagsEnd;
        }

        String += 1;
    }

ScanKeyFlagsEnd:
    *Argument = String;
    return;
}

INT
SortArrayAddElement (
    PSORT_ARRAY Array,
    PVOID Element
    )

/*++

Routine Description:

    This routine adds an element to the given array.

Arguments:

    Array - Supplies a pointer to the array to add an element to.

    Element - Supplies the element to add.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PVOID NewBuffer;
    UINTN NewCapacity;

    if (Array->Size == Array->Capacity) {
        if (Array->Capacity == 0) {
            NewCapacity = SORT_INITIAL_ELEMENT_COUNT;

        } else {
            NewCapacity = Array->Capacity * 2;
        }

        NewBuffer = realloc(Array->Data, NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        Array->Data = NewBuffer;
        Array->Capacity = NewCapacity;
    }

    Array->Data[Array->Size] = Element;
    Array->Size += 1;
    return 0;
}

INT
SortStringAddCharacter (
    PSORT_STRING String,
    CHAR Character
    )

/*++

Routine Description:

    This routine adds a character to the given string.

Arguments:

    String - Supplies a pointer to the string to add a character to.

    Character - Supplies the character to add.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PVOID NewBuffer;
    UINTN NewCapacity;

    if (String->Size == String->Capacity) {
        if (String->Capacity == 0) {
            NewCapacity = SORT_INITIAL_STRING_SIZE;

        } else {
            NewCapacity = String->Capacity * 2;
        }

        NewBuffer = realloc(String->Data, NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        String->Data = NewBuffer;
        String->Capacity = NewCapacity;
    }

    String->Data[String->Size] = Character;
    String->Size += 1;
    return 0;
}

PSORT_STRING
SortCreateString (
    PSTR InitialData,
    UINTN InitialDataSize
    )

/*++

Routine Description:

    This routine creates a new string.

Arguments:

    InitialData - Supplies an optional pointer to an initial buffer to fill it
        with.

    InitialDataSize - Supplies the size of the initial data in bytes.

Return Value:

    Returns a pointer to the new string on success. The caller is responsible
    for destroying this string.

    NULL on allocation failure.

--*/

{

    INT Status;
    PSORT_STRING String;

    Status = ENOMEM;
    String = malloc(sizeof(SORT_STRING));
    if (String == NULL) {
        goto CreateStringEnd;
    }

    memset(String, 0, sizeof(SORT_STRING));
    if (InitialDataSize == 0) {
        Status = 0;
        goto CreateStringEnd;
    }

    String->Data = malloc(InitialDataSize);
    if (String->Data == NULL) {
        goto CreateStringEnd;
    }

    if (InitialData != NULL) {
        memcpy(String->Data, InitialData, InitialDataSize);
    }

    String->Size = InitialDataSize;
    String->Capacity = InitialDataSize;
    Status = 0;

CreateStringEnd:
    if (Status != 0) {
        if (String != NULL) {
            if (String->Data != NULL) {
                free(String->Data);
            }

            free(String);
            String = NULL;
        }
    }

    return String;
}

VOID
SortDestroyString (
    PSORT_STRING String
    )

/*++

Routine Description:

    This routine destroys a string.

Arguments:

    String - Supplies a pointer to the string to destroy.

Return Value:

    None.

--*/

{

    if (String->Data != NULL) {
        free(String->Data);
    }

    free(String);
    return;
}

VOID
SortDestroyArray (
    PSORT_ARRAY Array,
    PSORT_DESTROY_ARRAY_ELEMENT_ROUTINE DestroyElementRoutine
    )

/*++

Routine Description:

    This routine destroys an array, calling the given destroy routine on each
    element in the array.

Arguments:

    Array - Supplies a pointer to the array to destroy.

    DestroyElementRoutine - Supplies a pointer to the routine that gets called
        on each element to perform any necessary cleanup on the elements in the
        array.

Return Value:

    None.

--*/

{

    UINTN ElementIndex;

    for (ElementIndex = 0; ElementIndex < Array->Size; ElementIndex += 1) {
        DestroyElementRoutine(Array->Data[ElementIndex]);
    }

    if (Array->Data != NULL) {
        free(Array->Data);
    }

    Array->Data = NULL;
    Array->Size = 0;
    Array->Capacity = 0;
    return;
}

VOID
SortDestroyInput (
    PSORT_INPUT Input
    )

/*++

Routine Description:

    This routine destroys an input.

Arguments:

    Input - Supplies a pointer to the input to destroy.

Return Value:

    None.

--*/

{

    if ((Input->File != NULL) && (Input->File != stdout)) {
        fclose(Input->File);
    }

    if (Input->Line != NULL) {
        SortDestroyString(Input->Line);
    }

    free(Input);
    return;
}

VOID
SortGetFieldOffset (
    PSORT_STRING String,
    INT Separator,
    LONG Field,
    LONG Character,
    PULONG Offset
    )

/*++

Routine Description:

    This routine gets the offset within the string corresponding to the given
    field, character combination.

Arguments:

    String - Supplies a pointer to the string.

    Separator - Supplies the separator character, or -1 if no separator was
        set.

    Field - Supplies the one-based field number.

    Character - Supplies the one-based character within the field.

    Offset - Supplies a pointer where the offset within the string
        corresponding to the given field and character will be returned.

Return Value:

    None.

--*/

{

    LONG CharacterIndex;
    PSTR CurrentString;
    LONG FieldIndex;

    //
    // Check for a quick exit.
    //

    if ((Field == 1) && (Character == 1)) {
        *Offset = 0;
        return;
    }

    if (Field == -1) {
        *Offset = String->Size;
        return;
    }

    //
    // Advance through the fields.
    //

    CurrentString = String->Data;
    for (FieldIndex = 1; FieldIndex < Field; FieldIndex += 1) {
        if (Separator == -1) {
            while ((UINTN)CurrentString - (UINTN)String->Data < String->Size) {

                //
                // By default, a field is delimited by the space between a
                // non-blank and a blank.
                //

                if ((isspace(*CurrentString)) &&
                    (CurrentString != String->Data) &&
                    (!isspace(*(CurrentString - 1)))) {

                    break;
                }

                CurrentString += 1;
            }

            if ((UINTN)CurrentString - (UINTN)String->Data == String->Size) {
                *Offset = String->Size;
                return;
            }

        } else {
            CurrentString = strchr(CurrentString, Separator);
            if (CurrentString == NULL) {
                *Offset = String->Size;
                return;
            }
        }

        CurrentString += 1;
    }

    //
    // Advance through the characters.
    //

    for (CharacterIndex = 1;
         (CharacterIndex < Character) || (Character == -1);
         CharacterIndex += 1) {

        if (Separator == -1) {
            if ((isspace(*CurrentString)) &&
                (CurrentString != String->Data) &&
                (!isspace(*(CurrentString - 1)))) {

                break;
            }

        } else {
            if (*CurrentString == Separator) {
                break;
            }
        }

        if ((UINTN)CurrentString - (UINTN)String->Data == String->Size) {
            break;
        }

        CurrentString += 1;
    }

    *Offset = (UINTN)CurrentString - (UINTN)String->Data;
    return;
}

LONG
SortStringToLong (
    PSORT_STRING String,
    ULONG Options,
    ULONG Offset
    )

/*++

Routine Description:

    This routine converts the given string to a decimal number, ignoring
    thousands groupings and potentially non-printable characters.

Arguments:

    String - Supplies a pointer to the string.

    Options - Supplies the options governing the scan.

    Offset - Supplies the offset from the beginning of the string to start
        scanning from.

Return Value:

    Returns the integer representation of the string.

--*/

{

    CHAR Character;
    LONG Index;
    BOOL Negative;
    BOOL SeenSomething;
    LONG Value;

    Negative = FALSE;
    SeenSomething = FALSE;
    Value = 0;
    for (Index = Offset; Index < String->Size; Index += 1) {
        Character = String->Data[Index];
        if (((Options & SORT_OPTION_IGNORE_NONPRINTABLE) != 0) &&
            (!isprint(Character))) {

            continue;
        }

        if ((SeenSomething == FALSE) && (Character == '-')) {
            SeenSomething = TRUE;
            Negative = TRUE;
            continue;
        }

        SeenSomething = TRUE;
        if (Character == ',') {
            continue;
        }

        if (!isdigit(Character)) {
            break;
        }

        Value = (Value * 10) + (Character - '0');
    }

    if (Negative != FALSE) {
        Value = -Value;
    }

    return Value;
}


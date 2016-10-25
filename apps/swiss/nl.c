/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    nl.c

Abstract:

    This module implements the nl (number lines) utility.

Author:

    Evan Green 31-Mar-2015

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
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define NL_VERSION_MAJOR 1
#define NL_VERSION_MINOR 0

#define NL_USAGE                                                               \
    "usage: nl [options] files...\n"                                           \
    "The nl utility writes each file to standard out, with line numbers "      \
    "added.\nOptions are:\n"                                                   \
    "  -b, --body-numbering=style -- Use the given style for numbering body "  \
    "lines.\n"                                                                 \
    "  -d, --section-delimiter=cc -- Use the given two characters as a \n"     \
    "      section delimiter. If a second character is missing, it is \n"      \
    "      assumed to be ':'.\n"                                               \
    "  -f, --footer-numbering=style -- Use the given style for numbering \n"   \
    "      footer lines.\n"                                                    \
    "  -h, --header-numbering=style -- Use the given style for numbering \n"   \
    "      header lines.\n"                                                    \
    "  -i, --line-increment=number -- Increment by this value at each line.\n" \
    "  -l, --join-blank-lines=number -- Group a given number of blank lines \n"\
    "      as one.\n"                                                          \
    "  -n, --number-format=format -- Number lines according to the given \n"   \
    "      format.\n"                                                          \
    "  -p, --no-renumber -- Do not reset line numbers at logical pages.\n"     \
    "  -s, --number-separator=string -- Add the given string after a \n"       \
    "      line number.\n"                                                     \
    "  -v, --starting-line-number=number -- Start with the given line "        \
    "number.\n"                                                                \
    "  -w, --number-width=number -- Set the column width for the number "      \
    "column.\n"                                                                \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"     \
    "The default is -v1, -i1, -l1, -sTAB, -w6 -nrn -hn -bt -fn.\n"             \
    "Style can be one of:\n"                                                   \
    "  a -- Number all lines.\n"                                               \
    "  t -- Number only non-empty lines.\n"                                    \
    "  n -- Number no lines.\n"                                                \
    "  pBRE -- Only number lines that match a given regular expression.\n"     \
    "Format can be one of:\n"                                                  \
    "  ln -- Left justified, no leading zeros.\n"                              \
    "  rn -- Right justified, no leading zeros.\n"                             \
    "  rz -- Right justified, leading zeros.\n"

#define NL_OPTIONS_STRING "b:d:f:h:i:l:n:ps:v:w:HV"

#define NL_INITIAL_LINE_SIZE 1024

//
// Define application defaults.
//

#define NL_DEFAULT_STARTING_LINE 1
#define NL_DEFAULT_INCREMENT 1
#define NL_DEFAULT_JOIN_BLANKS 1
#define NL_DEFAULT_SEPARATOR "\t"
#define NL_DEFAULT_WIDTH 6
#define NL_DEFAULT_NUMBER_FORMAT "%*ld"
#define NL_DEFAULT_HEADER_STYLE NlStyleNumberNone
#define NL_DEFAULT_FOOTER_STYLE NlStyleNumberNone
#define NL_DEFAULT_BODY_STYLE NlStyleNumberNonEmpty
#define NL_DEFAULT_DELIMITER "\\:"

//
// Define application option flags.
//

//
// Set this flag not to reset when a new page is encountered.
//

#define NL_OPTION_NO_RENUMBER 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _NL_STYLE {
    NlStyleInvalid,
    NlStyleNumberNone,
    NlStyleNumberAll,
    NlStyleNumberNonEmpty,
    NlStyleNumberRegularExpression
} NL_STYLE, *PNL_STYLE;

typedef enum _NL_REGION {
    NlRegionHeader,
    NlRegionBody,
    NlRegionFooter,
    NlRegionCount
} NL_REGION, *PNL_REGION;

/*++

Structure Description:

    This structure defines the context for an instantiation of the nl
    (number lines) application.

Members:

    Options - Stores the application options. See NL_OPTION_* definitions.

    Styles - Stores an array of styles for each region.

    RegularExpressions - Stores an array of regular expressions, one for each
        region.

    StartingLine - Stores the first line number.

    Increment - Stores the line increment.

    JoinBlanks - Stores the number of blank lines to join as one.

    Width - Stores the line number column width.

    NumberFormat - Stores the line number printf format.

    SectionDelimiter - Stores the page delimiter.

    SectionDelimiterLength - Stores the length of the section delimiter.

    Separator - Stores the separator string that goes between the numbers
        and the line.

    SeparatorLength - Stores the length of the separator string.

    Region - Stores the current region.

    Line - Stores the current line number.

    BlankCount - Stores the number of consecutive blank lines seen.

--*/

typedef struct _NL_CONTEXT {
    ULONG Options;
    NL_STYLE Styles[NlRegionCount];
    regex_t RegularExpressions[NlRegionCount];
    LONG StartingLine;
    LONG Increment;
    LONG JoinBlanks;
    LONG Width;
    PSTR NumberFormat;
    PSTR SectionDelimiter;
    UINTN SectionDelimiterLength;
    PSTR Separator;
    UINTN SeparatorLength;
    NL_REGION Region;
    LONG Line;
    LONG BlankCount;
} NL_CONTEXT, *PNL_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
NlNumberLines (
    PNL_CONTEXT Context,
    FILE *File
    );

INT
NlReadLine (
    FILE *File,
    PSTR *Line,
    PUINTN LineCapacity,
    PUINTN LineSize
    );

NL_STYLE
NlParseStyle (
    PSTR Argument,
    regex_t *Expression
    );

NL_REGION
NlParseDelimiter (
    PNL_CONTEXT Context,
    PSTR Line,
    UINTN LineSize
    );

//
// -------------------------------------------------------------------- Globals
//

struct option NlLongOptions[] = {
    {"body-numbering", required_argument, 0, 'b'},
    {"section-delimiter", required_argument, 0, 'd'},
    {"footer-numbering", required_argument, 0, 'f'},
    {"header-numbering", required_argument, 0, 'h'},
    {"line-increment", required_argument, 0, 'i'},
    {"join-blank-lines", required_argument, 0, 'l'},
    {"number-format", required_argument, 0, 'n'},
    {"no-renumber", no_argument, 0, 'p'},
    {"number-separator", required_argument, 0, 's'},
    {"starting-line-number", required_argument, 0, 'v'},
    {"number-width", required_argument, 0, 'w'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
NlMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the nl (number lines) utility.

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
    NL_CONTEXT Context;
    CHAR Delimiter[3];
    FILE *File;
    INT Option;
    int Status;
    int TotalStatus;

    memset(&Context, 0, sizeof(NL_CONTEXT));
    Context.Styles[NlRegionHeader] = NL_DEFAULT_HEADER_STYLE;
    Context.Styles[NlRegionFooter] = NL_DEFAULT_FOOTER_STYLE;
    Context.Styles[NlRegionBody] = NL_DEFAULT_BODY_STYLE;
    Context.StartingLine = NL_DEFAULT_STARTING_LINE;
    Context.Increment = NL_DEFAULT_INCREMENT;
    Context.JoinBlanks = NL_DEFAULT_JOIN_BLANKS;
    Context.Width = NL_DEFAULT_WIDTH;
    Context.NumberFormat = NL_DEFAULT_NUMBER_FORMAT;
    Context.Separator = NL_DEFAULT_SEPARATOR;
    Context.SectionDelimiter = NL_DEFAULT_DELIMITER;
    Context.Region = NlRegionBody;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             NL_OPTIONS_STRING,
                             NlLongOptions,
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
            Context.Styles[NlRegionBody] =
                     NlParseStyle(optarg,
                                  &(Context.RegularExpressions[NlRegionBody]));

            if (Context.Styles[NlRegionBody] == NlStyleInvalid) {
                SwPrintError(0, optarg, "Invalid style");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'd':
            if (*optarg == '\0') {
                SwPrintError(0, NULL, "Empty delimiter");
                Status = 1;
                goto MainEnd;

            } else if (strlen(optarg) == 1) {
                Delimiter[0] = optarg[0];
                Delimiter[1] = ':';
                Delimiter[2] = '\0';
                Context.SectionDelimiter = Delimiter;

            } else {
                Context.SectionDelimiter = optarg;
            }

            break;

        case 'f':
            Context.Styles[NlRegionFooter] =
                   NlParseStyle(optarg,
                                &(Context.RegularExpressions[NlRegionFooter]));

            if (Context.Styles[NlRegionFooter] == NlStyleInvalid) {
                SwPrintError(0, optarg, "Invalid style");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'h':
            Context.Styles[NlRegionHeader] =
                   NlParseStyle(optarg,
                                &(Context.RegularExpressions[NlRegionHeader]));

            if (Context.Styles[NlRegionHeader] == NlStyleInvalid) {
                SwPrintError(0, optarg, "Invalid style");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'i':
            Context.Increment = strtol(optarg, &AfterScan, 10);
            if (optarg == AfterScan) {
                SwPrintError(0, optarg, "Invalid number");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'l':
            Context.JoinBlanks = strtol(optarg, &AfterScan, 10);
            if (optarg == AfterScan) {
                SwPrintError(0, optarg, "Invalid number");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'n':
            if (strcmp(optarg, "ln") == 0) {
                Context.NumberFormat = "%-*ld";

            } else if (strcmp(optarg, "rn") == 0) {
                Context.NumberFormat = "%*ld";

            } else if (strcmp(optarg, "rz") == 0) {
                Context.NumberFormat = "%0*ld";

            } else {
                SwPrintError(0, optarg, "Invalid format");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'p':
            Context.Options |= NL_OPTION_NO_RENUMBER;
            break;

        case 'v':
            Context.StartingLine = strtol(optarg, &AfterScan, 10);
            if (optarg == AfterScan) {
                SwPrintError(0, optarg, "Invalid number");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'w':
            Context.Width = strtol(optarg, &AfterScan, 10);
            if (optarg == AfterScan) {
                SwPrintError(0, optarg, "Invalid number");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'V':
            SwPrintVersion(NL_VERSION_MAJOR, NL_VERSION_MINOR);
            return 1;

        case 'H':
            printf(NL_USAGE);
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

    if (ArgumentIndex >= ArgumentCount) {
        Status = NlNumberLines(&Context, stdin);
        goto MainEnd;
    }

    Context.Line = Context.StartingLine;
    Context.SectionDelimiterLength = strlen(Context.SectionDelimiter);
    Context.SeparatorLength = strlen(Context.Separator);

    //
    // Loop through the arguments again and perform the moves.
    //

    Status = 0;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        if (strcmp(Argument, "-") == 0) {
            Status = NlNumberLines(&Context, stdin);

        } else {
            File = fopen(Argument, "r");
            if (File == NULL) {
                SwPrintError(errno, Argument, "Unable to open");
                TotalStatus = 1;
                continue;
            }

            Status = NlNumberLines(&Context, File);
            fclose(File);
        }

        if (Status != 0) {
            TotalStatus = Status;
        }
    }

MainEnd:
    if (Context.Styles[NlRegionHeader] == NlStyleNumberRegularExpression) {
        regfree(&(Context.RegularExpressions[NlRegionHeader]));
    }

    if (Context.Styles[NlRegionBody] == NlStyleNumberRegularExpression) {
        regfree(&(Context.RegularExpressions[NlRegionBody]));
    }

    if (Context.Styles[NlRegionFooter] == NlStyleNumberRegularExpression) {
        regfree(&(Context.RegularExpressions[NlRegionFooter]));
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
NlNumberLines (
    PNL_CONTEXT Context,
    FILE *File
    )

/*++

Routine Description:

    This routine runs the bulk of the nl utility.

Arguments:

    Context - Supplies a pointer to the application context.

    File - Supplies an open file handle to read from.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    regex_t *Expression;
    PSTR Line;
    UINTN LineCapacity;
    UINTN LineSize;
    NL_REGION NewRegion;
    BOOL ShouldNumber;
    INT Status;
    NL_STYLE Style;

    Line = NULL;
    LineCapacity = 0;
    while (TRUE) {
        Status = NlReadLine(File, &Line, &LineCapacity, &LineSize);
        if (Status != 0) {
            break;
        }

        if (LineSize == 0) {
            break;
        }

        NewRegion = NlParseDelimiter(Context, Line, LineSize);
        if (NewRegion != NlRegionCount) {
            Context->Region = NewRegion;
            putc('\n', stdout);
            if (NewRegion == NlRegionHeader) {
                if ((Context->Options & NL_OPTION_NO_RENUMBER) == 0) {
                    Context->Line = Context->StartingLine;
                    Context->BlankCount = 0;
                }
            }

            continue;
        }

        Style = Context->Styles[Context->Region];
        Expression = &(Context->RegularExpressions[Context->Region]);
        ShouldNumber = FALSE;
        switch (Style) {
        case NlStyleNumberAll:
            ShouldNumber = TRUE;
            if ((LineSize == 0) || ((LineSize == 1) && (*Line == '\n'))) {
                Context->BlankCount += 1;
                if (Context->BlankCount == Context->JoinBlanks) {
                    ShouldNumber = TRUE;

                } else {
                    ShouldNumber = FALSE;
                }
            }

            break;

        case NlStyleNumberNone:
            break;

        case NlStyleNumberNonEmpty:
            ShouldNumber = TRUE;
            if ((LineSize == 0) || ((LineSize == 1) && (*Line == '\n'))) {
                ShouldNumber = FALSE;
            }

            break;

        case NlStyleNumberRegularExpression:
            Status = regexec(Expression, Line, 0, NULL, 0);
            if (Status == 0) {
                ShouldNumber = TRUE;
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        if (ShouldNumber != FALSE) {
            Context->BlankCount = 0;
            printf(Context->NumberFormat,
                   Context->Width,
                   (long int)Context->Line);

            printf("%s", Context->Separator);
            Context->Line += Context->Increment;

        } else {
            printf("%*s",
                   (int)(Context->Width + Context->SeparatorLength),
                   "");
        }

        printf("%s", Line);
    }

    if (Line != NULL) {
        free(Line);
    }

    return Status;
}

INT
NlReadLine (
    FILE *File,
    PSTR *Line,
    PUINTN LineCapacity,
    PUINTN LineSize
    )

/*++

Routine Description:

    This routine reads a line from the given file.

Arguments:

    File - Supplies a pointer to the file to read from.

    Line - Supplies a pointer to the line buffer, which may be reallocated.

    LineCapacity - Supplies a pointer that on input contains the size of the
        line buffer in bytes. If the buffer is resized, this variable will be
        updated.

    LineSize - Supplies a pointer where the line size will be returned, not
        including the null terminator.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Character;
    PVOID NewBuffer;
    UINTN NewCapacity;
    INT Status;

    Status = 0;
    *LineSize = 0;
    while (TRUE) {
        if (*LineSize + 1 >= *LineCapacity) {
            NewCapacity = *LineSize * 2;
            if (NewCapacity < NL_INITIAL_LINE_SIZE) {
                NewCapacity = NL_INITIAL_LINE_SIZE;
            }

            assert(NewCapacity > *LineSize + 1);

            NewBuffer = realloc(*Line, NewCapacity);
            if (NewBuffer == NULL) {
                return ENOMEM;
            }

            *Line = NewBuffer;
            *LineCapacity = NewCapacity;
        }

        Character = fgetc(File);
        if (Character == EOF) {
            if (ferror(File) != 0) {
                Status = errno;
            }

            break;
        }

        (*Line)[*LineSize] = Character;
        *LineSize += 1;
        if (Character == '\n') {
            break;
        }
    }

    (*Line)[*LineSize] = '\0';
    return Status;
}

NL_STYLE
NlParseStyle (
    PSTR Argument,
    regex_t *Expression
    )

/*++

Routine Description:

    This routine parses an nl utility style argument.

Arguments:

    Argument - Supplies the argument string.

    Expression - Supplies a pointer to the regular expression to compile if
        the argument turns out to be a regular expression.

Return Value:

    Returns a valid NL_STYLE on success.

    NlStyleInvalid on failure.

--*/

{

    INT Result;

    if (strcmp(Argument, "a") == 0) {
        return NlStyleNumberAll;

    } else if (strcmp(Argument, "t") == 0) {
        return NlStyleNumberNonEmpty;

    } else if (strcmp(Argument, "n") == 0) {
        return NlStyleNumberNone;
    }

    if (*Argument != 'p') {
        return NlStyleInvalid;
    }

    Result = regcomp(Expression, Argument + 1, 0);
    if (Result != 0) {
        return NlStyleInvalid;
    }

    return NlStyleNumberRegularExpression;
}

NL_REGION
NlParseDelimiter (
    PNL_CONTEXT Context,
    PSTR Line,
    UINTN LineSize
    )

/*++

Routine Description:

    This routine determines if this is the beginning of a new region: header,
    body, or footer.

Arguments:

    Context - Supplies a pointer to the application context.

    Line - Supplies the line to examine.

    LineSize - Supplies the size of the line.

Return Value:

    Returns a valid NL_REGION on success.

    NlRegionCount if this line does not delimit a new region.

--*/

{

    PSTR Delimiter;
    UINTN DelimiterLength;

    Delimiter = Context->SectionDelimiter;
    DelimiterLength = Context->SectionDelimiterLength;
    if (LineSize < DelimiterLength) {
        return NlRegionCount;
    }

    if (strncmp(Line, Delimiter, DelimiterLength) != 0) {
        return NlRegionCount;
    }

    Line += DelimiterLength;
    LineSize -= DelimiterLength;
    if ((*Line == '\n') || (*Line == '\0')) {
        return NlRegionFooter;
    }

    if (LineSize < DelimiterLength) {
        return NlRegionCount;
    }

    if (strncmp(Line, Delimiter, DelimiterLength) != 0) {
        return NlRegionCount;
    }

    Line += DelimiterLength;
    LineSize -= DelimiterLength;
    if ((*Line == '\n') || (*Line == '\0')) {
        return NlRegionBody;
    }

    if (LineSize < DelimiterLength) {
        return NlRegionCount;
    }

    if (strncmp(Line, Delimiter, DelimiterLength) != 0) {
        return NlRegionCount;
    }

    Line += DelimiterLength;
    LineSize -= DelimiterLength;
    if ((*Line == '\n') || (*Line == '\0')) {
        return NlRegionHeader;
    }

    return NlRegionCount;
}


/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    grep.c

Abstract:

    This module implements support for the grep utility.

Author:

    Evan Green 17-Jul-2013

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
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define GREP_VERSION_MAJOR 1
#define GREP_VERSION_MINOR 0
#define GREP_USAGE                                                             \
    "usage: grep [-E | -F][-c | -l | -q][-insvx] [-e pattern_list]...\n"       \
    "       [-f pattern_file]...[file]...\n"                                   \
    "       grep [-E | -F][-c | -l | -q][-insvx] pattern_list [file]...\n"     \
    "The grep utility searches for a text pattern in one or more text files.\n"\
    "Options are:\n"                                                           \
    "  -E, --extended-regexp -- Use extended regular expressions.\n"           \
    "  -F, --fixed-strings -- Use fixed strings, not regular expressions.\n"   \
    "  -c, --count -- Write only a count of selected lines to standard out.\n" \
    "  -e, --regexp pattern_list -- Specifies the pattern list to search "     \
    "for.\n"                                                                   \
    "  -f, --file pattern_file -- Specifies a file containing patterns to \n"  \
    "      search for.\n"                                                      \
    "  -H, --with-filename -- Print the filename for each match.\n"            \
    "  -h, --no-filename -- Do not print the filename for each match.\n"       \
    "  -i, --ignore-case -- Ignore case when searching.\n"                     \
    "  -l, --files-with-matches -- Write only the names of the files \n"       \
    "      searched and matched.\n"                                            \
    "  -n, --line-number -- Write the line number before each match.\n"        \
    "  -q, --quiet, --silent -- Quiet, write nothing to standard out.\n"       \
    "  -R, -r, --recursive -- Scan the contents of any directories found.\n"   \
    "  -s, --no-messages -- Suppress errors for nonexistant and unreadable "   \
    "files.\n"                                                                 \
    "  -v, --invert-match -- Select lines NOT matching any of the specified "  \
    "patterns.\n"                                                              \
    "  -x, --line-regexp -- Consider only input lines that use all \n"         \
    "        characters in the line to match the pattern.\n"                   \
    "  --help -- Show this help.\n"                                            \
    "  --version -- Show the version information.\n"

#define GREP_OPTIONS_STRING "EFce:f:HhilnqRrsvxV"
#define GREP_HELP 256

//
// Define the chunk size grep reads in.
//

#define GREP_READ_BLOCK_SIZE 1024

#define GREP_INITIAL_LINE_SIZE 16

//
// Define grep options.
//

//
// Set this option to use extended regular expressions.
//

#define GREP_OPTION_EXTENDED_EXPRESSIONS 0x00000001

//
// Set this option to match using fixed strings, not regular expressions.
//

#define GREP_OPTION_FIXED_STRINGS 0x00000002

//
// Set this option to print only the count of selected lines.
//

#define GREP_OPTION_LINE_COUNT 0x00000004

//
// Set this option to ignore case within a pattern.
//

#define GREP_OPTION_IGNORE_CASE 0x00000008

//
// Set this option to print the file name with each match.
//

#define GREP_OPTION_PRINT_FILE_NAMES 0x00000010

//
// Set this option to write line numbers for each match.
//

#define GREP_OPTION_PRINT_LINE_NUMBERS 0x00000020

//
// Set this option to suppress all output.
//

#define GREP_OPTION_QUIET 0x00000040

//
// Set this option to suppress errors for nonexistant and unreadable files.
//

#define GREP_OPTION_SUPPRESS_BLAND_ERRORS 0x00000080

//
// Set this option to select only lines that do not match any pattern.
//

#define GREP_OPTION_NEGATE_SEARCH 0x00000100

//
// Set this option to consider only input lines that use all characters in
// the line to match the pattern.
//

#define GREP_OPTION_FULL_LINE_ONLY 0x00000200

//
// Set this option to scan inside directories.
//

#define GREP_OPTION_RECURSIVE 0x00000400

//
// Set this option to suppress printing the match itself.
//

#define GREP_OPTION_SUPPRESS_MATCH_PRINT 0x00000800

//
// Define the maximum recursion depth for traversing into directories.
//

#define GREP_MAX_RECURSION_DEPTH 300

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a grep input file.

Members:

    ListEntry - Stores pointers to the next and previous input entries.

    FileName - Stores the name of the file.

    File - Stores the open file pointer, or NULL if the file could not be
        opened.

    Binary - Stores a boolean indicating if this file is a binray file or not.

--*/

typedef struct _GREP_INPUT {
    LIST_ENTRY ListEntry;
    PSTR FileName;
    FILE *File;
    BOOL Binary;
} GREP_INPUT, *PGREP_INPUT;

/*++

Structure Description:

    This structure defines a grep input file.

Members:

    ListEntry - Stores pointers to the next and previous pattern entries.

    Pattern - Stores the pattern string.

    Expression - Stores the regular expression structure.

--*/

typedef struct _GREP_PATTERN {
    LIST_ENTRY ListEntry;
    PSTR Pattern;
    regex_t Expression;
} GREP_PATTERN, *PGREP_PATTERN;

/*++

Structure Description:

    This structure defines the context for an instantiation of the grep
    utility.

Members:

    InputList - Stores the head of the list of input files to process.

    PatternList - Stores the list of patterns to search.

    Options - Stores the application options. See GREP_OPTION_* definitions.

--*/

typedef struct _GREP_CONTEXT {
    LIST_ENTRY InputList;
    LIST_ENTRY PatternList;
    ULONG Options;
} GREP_CONTEXT, *PGREP_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
GrepParsePatternFile (
    PGREP_CONTEXT Context,
    PSTR Path
    );

INT
GrepParsePatternList (
    PGREP_CONTEXT Context,
    PSTR String
    );

INT
GrepCompileRegularExpressions (
    PGREP_CONTEXT Context
    );

INT
GrepAddInputFile (
    PGREP_CONTEXT Context,
    PSTR Path,
    ULONG RecursionLevel
    );

INT
GrepProcessInput (
    PGREP_CONTEXT Context
    );

INT
GrepProcessInputEntry (
    PGREP_CONTEXT Context,
    PGREP_INPUT Input,
    PSTR *Buffer,
    size_t *BufferSize
    );

INT
GrepReadLine (
    PGREP_CONTEXT Context,
    PGREP_INPUT Input,
    PSTR *Buffer,
    size_t *BufferSize
    );

BOOL
GrepMatchPattern (
    PGREP_CONTEXT Context,
    PSTR Input,
    PGREP_PATTERN Pattern
    );

BOOL
GrepMatchFixedString (
    PGREP_CONTEXT Context,
    PSTR Input,
    PGREP_PATTERN Pattern
    );

//
// -------------------------------------------------------------------- Globals
//

struct option GrepLongOptions[] = {
    {"extended-regexp", no_argument, 0, 'E'},
    {"fixed-strings", no_argument, 0, 'F'},
    {"count", no_argument, 0, 'c'},
    {"regexp", required_argument, 0, 'e'},
    {"file", required_argument, 0, 'f'},
    {"with-filename", no_argument, 0, 'H'},
    {"no-filename", no_argument, 0, 'h'},
    {"ignore-case", no_argument, 0, 'i'},
    {"files-with-matches", no_argument, 0, 'l'},
    {"line-number", no_argument, 0, 'n'},
    {"quiet", no_argument, 0, 'q'},
    {"recursive", no_argument, 0, 'R'},
    {"silent", no_argument, 0, 'q'},
    {"no-messages", no_argument, 0, 's'},
    {"invert-match", no_argument, 0, 'v'},
    {"line-regexp", no_argument, 0, 'x'},
    {"help", no_argument, 0, GREP_HELP},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
GrepMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the grep utility, which
    searches for a pattern within a file.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    GREP_CONTEXT Context;
    PSTR FirstSource;
    PGREP_INPUT InputEntry;
    INT Option;
    PGREP_PATTERN Pattern;
    BOOL PatternsRead;
    BOOL ReadFromStandardIn;
    PSTR SecondSource;
    INT Status;
    BOOL SuppressFileName;
    INT TotalStatus;

    memset(&Context, 0, sizeof(GREP_CONTEXT));
    INITIALIZE_LIST_HEAD(&(Context.InputList));
    INITIALIZE_LIST_HEAD(&(Context.PatternList));
    Status = 0;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    FirstSource = NULL;
    SecondSource = NULL;
    PatternsRead = FALSE;
    SuppressFileName = FALSE;
    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             GREP_OPTIONS_STRING,
                             GrepLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'E':
            Context.Options |= GREP_OPTION_EXTENDED_EXPRESSIONS;
            if ((Context.Options & GREP_OPTION_FIXED_STRINGS) != 0) {
                SwPrintError(0, NULL, "Conflicting matchers specified");
                Status = 2;
                goto MainEnd;
            }

            break;

        case 'F':
            Context.Options |= GREP_OPTION_FIXED_STRINGS;
            if ((Context.Options &
                 GREP_OPTION_EXTENDED_EXPRESSIONS) != 0) {

                SwPrintError(0, NULL, "Conflicting matchers specified");
                Status = 2;
                goto MainEnd;
            }

            break;

        case 'c':
            Context.Options |= GREP_OPTION_LINE_COUNT;
            break;

        case 'e':
            PatternsRead = TRUE;
            Argument = optarg;

            assert(Argument != NULL);

            Status = GrepParsePatternList(&Context, Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'f':
            PatternsRead = TRUE;
            Argument = optarg;

            assert(Argument != NULL);

            Status = GrepParsePatternFile(&Context, Argument);
            if (Status != 0) {
                goto MainEnd;
            }

            break;

        case 'h':
            Context.Options &= ~GREP_OPTION_PRINT_FILE_NAMES;
            SuppressFileName = TRUE;
            break;

        case 'H':
            Context.Options |= GREP_OPTION_PRINT_FILE_NAMES;
            break;

        case 'i':
            Context.Options |= GREP_OPTION_IGNORE_CASE;
            break;

        case 'l':
            Context.Options |= GREP_OPTION_PRINT_FILE_NAMES |
                               GREP_OPTION_SUPPRESS_MATCH_PRINT;

            break;

        case 'n':
            Context.Options |= GREP_OPTION_PRINT_LINE_NUMBERS;
            break;

        case 'q':
            Context.Options |= GREP_OPTION_QUIET;
            break;

        case 'r':
        case 'R':
            Context.Options |= GREP_OPTION_RECURSIVE;
            break;

        case 's':
            Context.Options |= GREP_OPTION_SUPPRESS_BLAND_ERRORS;
            break;

        case 'v':
            Context.Options |= GREP_OPTION_NEGATE_SEARCH;
            break;

        case 'x':
            Context.Options |= GREP_OPTION_FULL_LINE_ONLY;
            break;

        case 'V':
            SwPrintVersion(GREP_VERSION_MAJOR, GREP_VERSION_MINOR);
            return 1;

        case GREP_HELP:
            printf(GREP_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex < ArgumentCount) {
        FirstSource = Arguments[ArgumentIndex];
        if (ArgumentIndex + 1 < ArgumentCount) {
            SecondSource = Arguments[ArgumentIndex + 1];
        }
    }

    //
    // If there were no scripts read, the first non-control argument is the
    // script.
    //

    ReadFromStandardIn = TRUE;
    if (PatternsRead == FALSE) {
        if (FirstSource == NULL) {
            SwPrintError(0, NULL, "Argument expected. Try --help for usage");
            Status = 2;
            goto MainEnd;
        }

        Status = GrepParsePatternList(&Context, FirstSource);
        if (Status != 0) {
            goto MainEnd;
        }

        if (SecondSource != NULL) {
            ReadFromStandardIn = FALSE;
        }

    } else if (FirstSource != NULL) {
        ReadFromStandardIn = FALSE;
    }

    Status = GrepCompileRegularExpressions(&Context);
    if (Status != 0) {
        goto MainEnd;
    }

    if (ReadFromStandardIn != FALSE) {

        //
        // Create a single input entry for standard in.
        //

        InputEntry = malloc(sizeof(GREP_INPUT));
        if (InputEntry == NULL) {
            Status = ENOMEM;
            goto MainEnd;
        }

        InputEntry->File = stdin;
        InputEntry->FileName = strdup("(standard in)");
        if (InputEntry->FileName == NULL) {
            Status = ENOMEM;
            goto MainEnd;
        }

        InputEntry->Binary = FALSE;
        INSERT_BEFORE(&(InputEntry->ListEntry), &(Context.InputList));
        Status = GrepProcessInput(&Context);
        goto MainEnd;
    }

    //
    // Loop through the remaining arguments to create the input entries.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;

        //
        // Skip over the script itself.
        //

        if ((PatternsRead == FALSE) && (Argument == FirstSource)) {
            continue;
        }

        Status = GrepAddInputFile(&Context, Argument, 0);
        if (Status != 0) {
            TotalStatus = Status;
        }
    }

    //
    // If there are multiple files, print the file names, unless explicitly
    // told not to.
    //

    if ((Context.InputList.Next != Context.InputList.Previous) &&
        (SuppressFileName == FALSE)) {

        Context.Options |= GREP_OPTION_PRINT_FILE_NAMES;
    }

    //
    // Let grep process all this.
    //

    Status = GrepProcessInput(&Context);

MainEnd:
    if (TotalStatus != 0) {
        Status = TotalStatus;
    }

    while (LIST_EMPTY(&(Context.InputList)) == FALSE) {
        InputEntry = LIST_VALUE(Context.InputList.Next, GREP_INPUT, ListEntry);
        LIST_REMOVE(&(InputEntry->ListEntry));
        if ((InputEntry->File != stdin) && (InputEntry->File != NULL)) {
            fclose(InputEntry->File);
        }

        if (InputEntry->FileName != NULL) {
            free(InputEntry->FileName);
        }

        free(InputEntry);
    }

    while (LIST_EMPTY(&(Context.PatternList)) == FALSE) {
        Pattern = LIST_VALUE(Context.PatternList.Next, GREP_PATTERN, ListEntry);
        LIST_REMOVE(&(Pattern->ListEntry));
        if (Pattern->Pattern != NULL) {
            free(Pattern->Pattern);

        } else {
            regfree(&(Pattern->Expression));
        }

        free(Pattern);
    }

    return Status;
}

INT
EgrepMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the egrep utility, which
    searches for a pattern within a file. It is equivalent to grep -E.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT ArgumentIndex;
    PSTR *NewArguments;
    INT Result;

    assert(ArgumentCount >= 1);

    NewArguments = malloc((ArgumentCount + 2) * sizeof(PSTR));
    if (NewArguments == NULL) {
        return ENOMEM;
    }

    NewArguments[0] = Arguments[0];
    NewArguments[1] = "-E";
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        NewArguments[1 + ArgumentIndex] = Arguments[ArgumentIndex];
    }

    NewArguments[ArgumentCount + 1] = NULL;
    Result = GrepMain(ArgumentCount + 1, NewArguments);
    free(NewArguments);
    return Result;
}

INT
FgrepMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the fgrep utility, which
    searches for a pattern within a file. It is equivalent to grep -f.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT ArgumentIndex;
    PSTR *NewArguments;
    INT Result;

    assert(ArgumentCount >= 1);

    NewArguments = malloc((ArgumentCount + 2) * sizeof(PSTR));
    if (NewArguments == NULL) {
        return ENOMEM;
    }

    NewArguments[0] = Arguments[0];
    NewArguments[1] = "-F";
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        NewArguments[1 + ArgumentIndex] = Arguments[ArgumentIndex];
    }

    NewArguments[ArgumentCount + 1] = NULL;
    Result = GrepMain(ArgumentCount + 1, NewArguments);
    free(NewArguments);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
GrepParsePatternFile (
    PGREP_CONTEXT Context,
    PSTR Path
    )

/*++

Routine Description:

    This routine reads a pattern list file.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to a string containing the path of the file to
        read in.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PCHAR Buffer;
    size_t BufferSize;
    ssize_t BytesRead;
    int File;
    PCHAR NewBuffer;
    int Status;
    size_t TotalBytesRead;

    Buffer = NULL;
    File = -1;
    TotalBytesRead = 0;

    //
    // Allocate an initial buffer.
    //

    BufferSize = GREP_READ_BLOCK_SIZE;
    Buffer = malloc(GREP_READ_BLOCK_SIZE);
    if (Buffer == NULL) {
        Status = ENOMEM;
        goto ReadFileInEnd;
    }

    File = SwOpen(Path, O_RDONLY | O_BINARY, 0);
    if (File < 0) {
        if ((Context->Options & GREP_OPTION_SUPPRESS_BLAND_ERRORS) != 0) {
            Status = 0;

        } else {
            Status = errno;
        }

        goto ReadFileInEnd;
    }

    //
    // Loop reading the entire pattern file in.
    //

    while (TRUE) {
        do {
            BytesRead = read(File,
                             Buffer + TotalBytesRead,
                             BufferSize - TotalBytesRead - 1);

        } while ((BytesRead < 0) && (errno == EINTR));

        if (BytesRead < 0) {
            Status = errno;
            goto ReadFileInEnd;
        }

        TotalBytesRead += BytesRead;
        if (BytesRead == 0) {
            break;
        }

        //
        // If there's not at least a block's worth in the buffer, double the
        // buffer size.
        //

        if (BufferSize - TotalBytesRead < GREP_READ_BLOCK_SIZE) {
            BufferSize *= 2;
            NewBuffer = realloc(Buffer, BufferSize);
            if (NewBuffer != NULL) {
                Buffer = NewBuffer;

            } else {
                Status = ENOMEM;
                goto ReadFileInEnd;
            }
        }
    }

    //
    // Null terminate the string. The loop ensures there's at least one more
    // space open.
    //

    assert(TotalBytesRead < BufferSize);

    Buffer[TotalBytesRead] = '\0';

    //
    // If something was read, parse it.
    //

    if (TotalBytesRead != 0) {
        Status = GrepParsePatternList(Context, Buffer);
        if (Status != 0) {
            goto ReadFileInEnd;
        }
    }

    Status = 0;

ReadFileInEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Status;
}

INT
GrepParsePatternList (
    PGREP_CONTEXT Context,
    PSTR String
    )

/*++

Routine Description:

    This routine reads in a string, splits it on newlines, and creates
    pattern entries for it.

Arguments:

    Context - Supplies a pointer to the application context.

    String - Supplies the string to split.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR CurrentLine;
    size_t LineLength;
    PSTR NextLine;
    PGREP_PATTERN Pattern;
    INT Status;

    Pattern = NULL;

    //
    // Loop splitting lines.
    //

    CurrentLine = String;
    while (TRUE) {
        NextLine = strchr(CurrentLine, '\n');
        if (NextLine != NULL) {
            LineLength = (UINTN)NextLine - (UINTN)CurrentLine;

        } else {
            LineLength = strlen(CurrentLine);
            if (LineLength == 0) {
                break;
            }
        }

        Pattern = malloc(sizeof(GREP_PATTERN));
        if (Pattern == NULL) {
            Status = ENOMEM;
            goto ParsePatternListEnd;
        }

        memset(Pattern, 0, sizeof(GREP_PATTERN));
        Pattern->Pattern = malloc(LineLength + 1);
        if (Pattern->Pattern == NULL) {
            Status = ENOMEM;
            goto ParsePatternListEnd;
        }

        if (LineLength != 0) {
            memcpy(Pattern->Pattern, CurrentLine, LineLength);
        }

        Pattern->Pattern[LineLength] = '\0';
        INSERT_BEFORE(&(Pattern->ListEntry), &(Context->PatternList));
        Pattern = NULL;
        if (NextLine == NULL) {
            break;
        }

        CurrentLine = NextLine + 1;
    }

    Status = 0;

ParsePatternListEnd:
    if (Pattern != NULL) {
        if (Pattern->Pattern != NULL) {
            free(Pattern->Pattern);
        }

        free(Pattern);
    }

    return Status;
}

INT
GrepCompileRegularExpressions (
    PGREP_CONTEXT Context
    )

/*++

Routine Description:

    This routine compiles all regular expression patterns if appropriate.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT CompileFlags;
    PLIST_ENTRY CurrentEntry;
    PSTR ErrorString;
    size_t ErrorStringSize;
    PGREP_PATTERN Pattern;
    INT Status;

    //
    // Skip this if they're just fixed strings and not regular expressions.
    //

    if ((Context->Options & GREP_OPTION_FIXED_STRINGS) != 0) {
        return 0;
    }

    //
    // Figure out the compile flags.
    //

    CompileFlags = REG_NOSUB;
    if ((Context->Options & GREP_OPTION_EXTENDED_EXPRESSIONS) != 0) {
        CompileFlags |= REG_EXTENDED;
    }

    if ((Context->Options & GREP_OPTION_IGNORE_CASE) != 0) {
        CompileFlags |= REG_ICASE;
    }

    CurrentEntry = Context->PatternList.Next;
    while (CurrentEntry != &(Context->PatternList)) {
        Pattern = LIST_VALUE(CurrentEntry, GREP_PATTERN, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        Status = regcomp(&(Pattern->Expression),
                         Pattern->Pattern,
                         CompileFlags);

        if (Status != 0) {
            ErrorStringSize = regerror(Status, &(Pattern->Expression), NULL, 0);
            ErrorString = malloc(ErrorStringSize);
            if (ErrorString != NULL) {
                regerror(Status,
                         &(Pattern->Expression),
                         ErrorString,
                         ErrorStringSize);

                SwPrintError(0,
                             NULL,
                             "Invalid regular expression '%s': %s",
                             Pattern->Pattern,
                             ErrorString);

                Status = 3;
                goto CompileRegularExpressionsEnd;
            }
        }

        //
        // Free the pattern both because it's no longer needed and to indicate
        // there's a valid compiled regular expression there.
        //

        free(Pattern->Pattern);
        Pattern->Pattern = NULL;
    }

    Status = 0;

CompileRegularExpressionsEnd:
    return Status;
}

INT
GrepAddInputFile (
    PGREP_CONTEXT Context,
    PSTR Path,
    ULONG RecursionLevel
    )

/*++

Routine Description:

    This routine adds a file to the list of files grep should process.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the file path to add.

    RecursionLevel - Supplies the recursion depth of this function.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    DIR *Directory;
    struct dirent *Entry;
    PGREP_INPUT InputEntry;
    struct stat Stat;
    INT Status;
    INT TotalStatus;

    Directory = NULL;
    InputEntry = NULL;
    TotalStatus = 0;
    Status = SwStat(Path, TRUE, &Stat);
    if (Status != 0) {
        Status = errno;
        SwPrintError(Status, Path, "Unable to stat");
        goto AddInputFileEnd;
    }

    if (S_ISDIR(Stat.st_mode)) {

        //
        // Skip it unless recursive mode is on.
        //

        if ((Context->Options & GREP_OPTION_RECURSIVE) == 0) {
            Status = 0;
            goto AddInputFileEnd;
        }

        if (RecursionLevel >= GREP_MAX_RECURSION_DEPTH) {
            SwPrintError(Status, Path, "Max recursion depth reached");
            Status = ELOOP;
            goto AddInputFileEnd;
        }

        Directory = opendir(Path);
        if (Directory == NULL) {
            Status = errno;
            SwPrintError(Status, Path, "Unable to open directory");
            goto AddInputFileEnd;
        }

        //
        // Loop through all entries in the directory.
        //

        while (TRUE) {
            errno = 0;
            Entry = readdir(Directory);
            if (Entry == NULL) {
                Status = errno;
                if (Status != 0) {
                    SwPrintError(Status, Path, "Unable to read directory");
                    goto AddInputFileEnd;
                }

                break;
            }

            if ((strcmp(Entry->d_name, ".") == 0) ||
                (strcmp(Entry->d_name, "..") == 0)) {

                continue;
            }

            Status = SwAppendPath(Path,
                                  strlen(Path) + 1,
                                  Entry->d_name,
                                  strlen(Entry->d_name) + 1,
                                  &AppendedPath,
                                  &AppendedPathSize);

            if (Status == FALSE) {
                Status = ENOMEM;
                goto AddInputFileEnd;
            }

            Status = GrepAddInputFile(Context,
                                      AppendedPath,
                                      RecursionLevel + 1);

            free(AppendedPath);
            if (Status != 0) {
                TotalStatus = Status;
            }
        }

    //
    // This is not a directory, add it as an input.
    //

    } else {
        InputEntry = malloc(sizeof(GREP_INPUT));
        if (InputEntry == NULL) {
            Status = ENOMEM;
            goto AddInputFileEnd;
        }

        memset(InputEntry, 0, sizeof(GREP_INPUT));
        InputEntry->FileName = strdup(Path);
        if (InputEntry->FileName == NULL) {
            Status = ENOMEM;
            goto AddInputFileEnd;
        }

        InputEntry->Binary = FALSE;
        INSERT_BEFORE(&(InputEntry->ListEntry), &(Context->InputList));
        InputEntry = NULL;
    }

    Status = 0;

AddInputFileEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    if (TotalStatus != 0) {
        Status = TotalStatus;
    }

    return Status;
}

INT
GrepProcessInput (
    PGREP_CONTEXT Context
    )

/*++

Routine Description:

    This routine compiles all regular expression patterns if appropriate.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    BOOL FileOpened;
    PGREP_INPUT Input;
    PSTR LineBuffer;
    size_t LineBufferSize;
    INT Status;
    INT TotalStatus;

    LineBuffer = NULL;
    LineBufferSize = 0;
    TotalStatus = 1;

    //
    // Just loop through each input.
    //

    CurrentEntry = Context->InputList.Next;
    while (CurrentEntry != &(Context->InputList)) {
        Input = LIST_VALUE(CurrentEntry, GREP_INPUT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FileOpened = FALSE;
        if (Input->File == NULL) {
            Input->File = fopen(Input->FileName, "r");
            if (Input->File == NULL) {
                if ((Context->Options &
                     GREP_OPTION_SUPPRESS_BLAND_ERRORS) == 0) {

                    Status = errno;
                    SwPrintError(Status, Input->FileName, "Unable to open");
                    TotalStatus = 2;
                    continue;
                }
            }

            FileOpened = TRUE;
        }

        Status = GrepProcessInputEntry(Context,
                                       Input,
                                       &LineBuffer,
                                       &LineBufferSize);

        if (FileOpened != FALSE) {
            fclose(Input->File);
            Input->File = NULL;
        }

        if (Status == 0) {
            if (TotalStatus == 1) {
                TotalStatus = 0;
            }

        } else if (Status > 1) {
            TotalStatus = Status;
        }
    }

    if (LineBuffer != NULL) {
        free(LineBuffer);
    }

    return TotalStatus;
}

INT
GrepProcessInputEntry (
    PGREP_CONTEXT Context,
    PGREP_INPUT Input,
    PSTR *Buffer,
    size_t *BufferSize
    )

/*++

Routine Description:

    This routine compiles all regular expression patterns if appropriate.

Arguments:

    Context - Supplies a pointer to the application context.

    Input - Supplies a pointer to the input entry.

    Buffer - Supplies a pointer that on input contains a buffer. On output,
        returns a potentially reallocated buffer.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the potentially expanded size of the
        buffer.

Return Value:

    0 if the input matched.

    1 if the input did not match.

    Other error codes on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONG LineNumber;
    BOOL Match;
    ULONG MatchCount;
    PGREP_PATTERN Pattern;
    INT Status;

    LineNumber = 1;
    MatchCount = 0;

    //
    // Loop across every line.
    //

    while (TRUE) {
        Status = GrepReadLine(Context, Input, Buffer, BufferSize);
        if (Status == EOF) {
            Status = 0;
            break;

        } else if (Status != 0) {
            goto ProcessInputEntryEnd;
        }

        CurrentEntry = Context->PatternList.Next;
        while (CurrentEntry != &(Context->PatternList)) {
            Pattern = LIST_VALUE(CurrentEntry, GREP_PATTERN, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            Match = GrepMatchPattern(Context, *Buffer, Pattern);
            if (Match != FALSE) {
                MatchCount += 1;
            }

            //
            // If it didn't match, keep going.
            //

            if (Match == FALSE) {
                continue;
            }

            if ((Context->Options & GREP_OPTION_QUIET) != 0) {
                continue;
            }

            //
            // If there are more than one file elements and the file name was
            // not already printed, precede the match with the file name.
            //

            if ((Context->Options & GREP_OPTION_PRINT_FILE_NAMES) != 0) {
                if ((Context->Options &
                     GREP_OPTION_SUPPRESS_MATCH_PRINT) != 0) {

                    printf("%s\n", Input->FileName);
                    break;

                } else {
                    printf("%s:", Input->FileName);
                }
            }

            //
            // With line counts only, just keep going.
            //

            if ((Context->Options & GREP_OPTION_LINE_COUNT) != 0) {
                continue;
            }

            if (Input->Binary != FALSE) {
                printf("Binary file %s matches.\n", Input->FileName);
                break;
            }

            //
            // If a line number is desired, print that too.
            //

            if ((Context->Options & GREP_OPTION_PRINT_LINE_NUMBERS) != 0) {
                printf("%d:", LineNumber);
            }

            //
            // Print the line itself.
            //

            printf("%s\n", *Buffer);
        }

        LineNumber += 1;
        if ((MatchCount != 0) &&
            ((Input->Binary != FALSE) ||
             ((Context->Options & GREP_OPTION_SUPPRESS_MATCH_PRINT) != 0))) {

            break;
        }
    }

    //
    // Print the count if desired.
    //

    if (((Context->Options & GREP_OPTION_LINE_COUNT) != 0) &&
        ((Context->Options & GREP_OPTION_QUIET) == 0) &&
        ((Context->Options & GREP_OPTION_SUPPRESS_MATCH_PRINT) == 0)) {

        printf("%d\n", MatchCount);
    }

    Status = 0;

ProcessInputEntryEnd:
    if ((Status == 0) && (MatchCount == 0)) {
        Status = 1;
    }

    return Status;
}

INT
GrepReadLine (
    PGREP_CONTEXT Context,
    PGREP_INPUT Input,
    PSTR *Buffer,
    size_t *BufferSize
    )

/*++

Routine Description:

    This routine reads a new line into the given buffer.

Arguments:

    Context - Supplies a pointer to the application context.

    Input - Supplies a pointer to the input to read from.

    Buffer - Supplies a pointer that on input contains a buffer. On output,
        returns the buffer containing the line, which may be reallocated from
        the original.

    BufferSize - Supplies a pointer that on input contains the size of the
        input buffer. On output, returns the potentially expanded size of the
        buffer. Note that this returns the size of the allocation, not the
        length of the string. The string is null terminated.

Return Value:

    0 on success.

    Non-zero on failure.

    EOF if the end of the file was hit.

--*/

{

    INT Character;
    size_t CharacterCount;
    PSTR NewBuffer;
    INT Status;
    PSTR String;
    size_t StringSize;

    CharacterCount = 0;
    String = *Buffer;
    StringSize = *BufferSize;

    //
    // Loop reading characters.
    //

    while (TRUE) {

        //
        // Ensure that the string is big enough to take two more characters.
        //

        if (CharacterCount + 2 > StringSize) {
            if (StringSize == 0) {
                StringSize = GREP_INITIAL_LINE_SIZE;

            } else {
                StringSize *= 2;
            }

            assert(StringSize >= CharacterCount + 2);

            NewBuffer = realloc(String, StringSize);
            if (NewBuffer == NULL) {
                Status = ENOMEM;
                goto ReadLineEnd;
            }

            String = NewBuffer;
        }

        //
        // Get a new character. If it's the end of the file, terminate this
        // line, or if this line is empty, return EOF overall.
        //

        Character = fgetc(Input->File);
        if (Character == EOF) {
            if (CharacterCount != 0) {
                break;
            }

            Status = EOF;
            goto ReadLineEnd;

        } else {
            if (Character == '\0') {
                Input->Binary = TRUE;
            }

            //
            // Skip over any null terminators at the beginning.
            //

            if ((Character == '\0') && (CharacterCount == 0)) {
                continue;
            }

            if ((Character == '\n') || (Character == '\0')) {
                break;

            } else {
                String[CharacterCount] = Character;
                CharacterCount += 1;
            }
        }
    }

    String[CharacterCount] = '\0';
    Status = 0;

ReadLineEnd:
    *Buffer = String;
    *BufferSize = StringSize;
    return Status;
}

BOOL
GrepMatchPattern (
    PGREP_CONTEXT Context,
    PSTR Input,
    PGREP_PATTERN Pattern
    )

/*++

Routine Description:

    This routine determines if the given input line matches a grep pattern,
    and prints out match information if it does.

Arguments:

    Context - Supplies a pointer to the application context.

    Input - Supplies a pointer to the null terminated input line.

    Pattern - Supplies a pointer to the pattern to match against.

Return Value:

    TRUE if the pattern matched the input.

    FALSE if there was no matched.

--*/

{

    regmatch_t ExpressionMatch;
    BOOL Match;
    INT Status;

    Match = FALSE;

    //
    // First figure out if the pattern matched.
    //

    if ((Context->Options & GREP_OPTION_FIXED_STRINGS) != 0) {
        Match = GrepMatchFixedString(Context, Input, Pattern);

    } else {
        Status = regexec(&(Pattern->Expression), Input, 1, &ExpressionMatch, 0);
        if (Status == 0) {
            Match = TRUE;
            if ((Context->Options & GREP_OPTION_FULL_LINE_ONLY) != 0) {
                if ((ExpressionMatch.rm_so != 0) ||
                    (Input[ExpressionMatch.rm_eo - 1] != '\0')) {

                    Match = FALSE;
                }
            }
        }
    }

    if ((Context->Options & GREP_OPTION_NEGATE_SEARCH) != 0) {
        Match = !Match;
    }

    return Match;
}

BOOL
GrepMatchFixedString (
    PGREP_CONTEXT Context,
    PSTR Input,
    PGREP_PATTERN Pattern
    )

/*++

Routine Description:

    This routine attempts to match against a fixed string pattern.

Arguments:

    Context - Supplies a pointer to the application context.

    Input - Supplies a pointer to the null terminated input line.

    Pattern - Supplies a pointer to the pattern to match against.

Return Value:

    TRUE if the pattern matched the input.

    FALSE if there was no matched.

--*/

{

    ULONG BeginIndex;
    BOOL IgnoreCase;
    BOOL Match;
    PSTR PatternString;
    ULONG SearchIndex;

    IgnoreCase = FALSE;
    if ((Context->Options & GREP_OPTION_IGNORE_CASE) != 0) {
        IgnoreCase = TRUE;
    }

    Match = FALSE;
    PatternString = Pattern->Pattern;
    SearchIndex = 0;
    BeginIndex = 0;
    while (Input[BeginIndex] != '\0') {
        SearchIndex = 0;

        //
        // This seems like a mess, but isn't so bad. Loop as long as:
        // 1. The pattern hasn't ended, AND
        // 2. Either:
        //    a. The pattern matches the input, OR
        //    b. "Ignore case" is on and the lowercase versions of the pattern
        //       and inputs match.
        //
        // See, not so bad.
        //

        while ((PatternString[SearchIndex] != '\0') &&
               ((Input[BeginIndex + SearchIndex] ==
                 PatternString[SearchIndex]) ||
                ((IgnoreCase != FALSE) &&
                 (tolower(Input[BeginIndex + SearchIndex]) ==
                  tolower(PatternString[SearchIndex]))))) {

            SearchIndex += 1;
        }

        if (PatternString[SearchIndex] == '\0') {
            Match = TRUE;
            break;
        }

        BeginIndex += 1;
    }

    //
    // If there's a match and it's required to use up the whole line, see that
    // it does. That case could have been optimized by stopping the loop above
    // early, but it's expected to be an uncommonly used flag, so adding the
    // extra compare in every loop iteration seemed worse.
    //

    if ((Match != FALSE) &&
        ((Context->Options & GREP_OPTION_FULL_LINE_ONLY) != 0)) {

        if ((BeginIndex != 0) || (Input[BeginIndex + SearchIndex] != '\0')) {
            Match = FALSE;
        }
    }

    return Match;
}


/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    diff.c

Abstract:

    This module implements the diff utility.

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

#define DIFF_VERSION_MAJOR 1
#define DIFF_VERSION_MINOR 0

#define DIFF_USAGE                                                             \
    "usage: diff [-c | -e | -f | -C n][-br] file1 file2\n"                     \
    "The diff utility compares the contents of two paths and reports the \n"   \
    "differences to standard out. Options are:\n"                              \
    "  -b, --ignore-space-change -- Ignore whitespace changes.\n"              \
    "  -c  -- Produce three lines of context around every diff.\n"             \
    "  -C, --context=n -- Produce n lines of context around every diff, \n"    \
    "      where n is a decimal integer.\n"                                    \
    "  --color=value -- Turn on or off color printing. Valid values are \n"    \
    "      always, never, and auto."                                           \
    "  -e, --ed -- Output an ed script.\n"                                     \
    "  -N, --new-file -- Treat absent files as empty.\n"                       \
    "  -r, --recursive -- Recursively compare any subdirectories found.\n"     \
    "  -u, --unified=n -- Produce a unified diff format, with n lines of \n"   \
    "      context.\n"                                                         \
    "  -x, --exclude=pattern -- exclude file that match the given pattern.\n"  \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Show the application version information and exit.\n"      \

#define DIFF_OPTIONS_STRING "bcC:eNru::x:"

//
// Define the diff option flags.
//

#define DIFF_OPTION_IGNORE_BLANKS 0x00000001
#define DIFF_OPTION_RECURSIVE     0x00000002
#define DIFF_OPTION_COLOR         0x00000004
#define DIFF_OPTION_ABSENT_EMPTY  0x00000008

//
// Define the default number of context lines when they're asked for.
//

#define DIFF_DEFAULT_CONTEXT_LINES 3

//
// Define the maximum depth of directories that diff will crawl down.
//

#define DIFF_MAX_RECURSION_DEPTH 100

//
// Define the initial size of a directory listing array in elements.
//

#define DIFF_INITIAL_ARRAY_CAPACITY 32

//
// Define the initial size of the line buffer in bytes.
//

#define DIFF_INITIAL_LINE_BUFFER 256

//
// Define the colors used for insertion and deletion.
//

#define DIFF_INSERTION_COLOR ConsoleColorDarkGreen
#define DIFF_DELETION_COLOR ConsoleColorDarkRed

//
// Define the time format string used by context diffs.
//

#define CONTEXT_DIFF_TIMESTAMP_FORMAT "%a %b %d %H:%M:%S %Y"
#define CONTEXT_DIFF_TIMESTAMP_SIZE 26

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DIFF_OUTPUT_TYPE {
    DiffOutputInvalid,
    DiffOutputDefault,
    DiffOutputEd,
    DiffOutputUnified,
} DIFF_OUTPUT_TYPE, *PDIFF_OUTPUT_TYPE;

typedef enum _DIFF_FILE_TYPE {
    DiffFileUnknown,
    DiffFileBlockDevice,
    DiffFileCharacterDevice,
    DiffFileDirectory,
    DiffFileRegularFile,
    DiffFileFifo,
    DiffFileLink,
    DiffFileSocket,
    DiffFileTypeCount
} DIFF_FILE_TYPE, *PDIFF_FILE_TYPE;

/*++

Structure Description:

    This structure defines a diff input line.

Members:

    Data - Stores a pointer to the allocated null terminated line.

    Size - Stores the size of the line data in bytes.

    Hash - Stores the hash of the file, which is used to quickly determine if
        two files are not equal (but not necessarily if they're equal).

    Modified - Stores a boolean indicating that this line is part of the diff.

--*/

typedef struct _DIFF_LINE {
    PSTR Data;
    UINTN Size;
    ULONG Hash;
    BOOL Modified;
} DIFF_LINE, *PDIFF_LINE;

/*++

Structure Description:

    This structure defines an input file of diff.

Members:

    Name - Stores a pointer to a string containing the name of the file.

    ModificationTime - Stores the files modification date.

    Type - Stores the type of file this thing is.

    Binary - Stores a boolean indicating if this file is binary data or text.

    NoNewlineAtEnd - Stores a boolean indicating if there is no newline at the
        end of the file.

    File - Stores a pointer to the file input handle.

    LineCount - Stores the number of lines in the file.

    Lines - Stores an array of pointers to the lines of the file.

--*/

typedef struct _DIFF_FILE {
    PSTR Name;
    time_t ModificationTime;
    DIFF_FILE_TYPE Type;
    BOOL Binary;
    BOOL NoNewlineAtEnd;
    FILE *File;
    INTN LineCount;
    PDIFF_LINE *Lines;
} DIFF_FILE, *PDIFF_FILE;

/*++

Structure Description:

    This structure defines the contents of a directory.

Members:

    FileCount - Stores the number of files in the directory.

    ArrayCapacity - Stores the size of the array allocation in elements.

    Files - Stores the array of pointers to diff files.

--*/

typedef struct _DIFF_DIRECTORY {
    UINTN FileCount;
    UINTN ArrayCapacity;
    PDIFF_FILE *Files;
} DIFF_DIRECTORY, *PDIFF_DIRECTORY;

/*++

Structure Description:

    This structure defines the context for an instantiation of the diff
    application.

Members:

    Options - Stores the bitfield of diff options.

    OutputType - Stores the type of output to produce.

    ContextLines - Stores the number of lines of context to produce around
        each diff.

    EmptyFile - Stores a pointer to a dummy file that can be used for
        comparisons.

    FileExclusions - Stores a pointer to an array of string containing patterns
        of file names to exclude.

    FileExclusionCount - Stores the number of elements in the file exclusions
        pattern.

--*/

typedef struct _DIFF_CONTEXT {
    ULONG Options;
    DIFF_OUTPUT_TYPE OutputType;
    LONG ContextLines;
    DIFF_FILE EmptyFile;
    PSTR *FileExclusions;
    UINTN FileExclusionCount;
} DIFF_CONTEXT, *PDIFF_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DiffComparePaths (
    PDIFF_CONTEXT Context,
    PSTR PathA,
    PSTR PathB
    );

INT
DiffCompareFiles (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB,
    ULONG RecursionLevel
    );

INT
DiffCompareDirectories (
    PDIFF_CONTEXT Context,
    PSTR PathA,
    PSTR PathB,
    ULONG RecursionLevel
    );

INT
DiffGetDirectoryListing (
    PDIFF_CONTEXT Context,
    PSTR Path,
    PDIFF_DIRECTORY *NewDirectory
    );

BOOL
DiffIsFileNameExcluded (
    PDIFF_CONTEXT Context,
    PSTR Name
    );

VOID
DiffDestroyDirectory (
    PDIFF_CONTEXT Context,
    PDIFF_DIRECTORY Directory
    );

INT
DiffCreateFile (
    PDIFF_CONTEXT Context,
    PSTR Directory,
    PSTR Path,
    PDIFF_FILE *File
    );

VOID
DiffDestroyFile (
    PDIFF_CONTEXT Context,
    PDIFF_FILE File
    );

VOID
DiffDestroyLine (
    PDIFF_LINE Line
    );

DIFF_FILE_TYPE
DiffGetFileType (
    mode_t Mode
    );

int
DiffCompareFileNames (
    const void *LeftPointer,
    const void *RightPointer
    );

VOID
DiffPrintCommandLine (
    PDIFF_CONTEXT Context,
    PSTR PathA,
    PDIFF_FILE FileA,
    PSTR PathB,
    PDIFF_FILE FileB
    );

INT
DiffLoadFile (
    PDIFF_CONTEXT Context,
    PSTR Directory,
    PDIFF_FILE File
    );

INT
DiffCompareRegularFiles (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB,
    ULONG RecursionLevel
    );

INT
DiffCompareBinaryFiles (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB
    );

INT
DiffComputeLongestCommonSubsequence (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    INTN LowerA,
    INTN UpperA,
    INTN LowerB,
    INTN UpperB,
    PINTN DownVector,
    PINTN UpVector
    );

VOID
DiffComputeShortestMiddleSnake (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    INTN LowerA,
    INTN UpperA,
    INTN LowerB,
    INTN UpperB,
    PINTN DownVector,
    PINTN UpVector,
    PINTN MiddleSnakeX,
    PINTN MiddleSnakeY
    );

INT
DiffCompareLines (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    UINTN LineIndexA,
    UINTN LineIndexB
    );

VOID
DiffPrintEdDiffs (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB
    );

VOID
DiffPrintContextDiffs (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB
    );

VOID
DiffPrintUnifiedDiffs (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB
    );

VOID
DiffFindNextHunk (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    PINTN LineA,
    PINTN LineB,
    PINTN SizeA,
    PINTN SizeB
    );

//
// -------------------------------------------------------------------- Globals
//

struct option DiffLongOptions[] = {
    {"ignore-space-change", no_argument, 0, 'b'},
    {"context", optional_argument, 0, 'C'},
    {"color", required_argument, 0, '1'},
    {"ed", no_argument, 0, 'e'},
    {"new-file", no_argument, 0, 'N'},
    {"recursive", no_argument, 0, 'r'},
    {"unified", optional_argument, 0, 'u'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

PSTR DiffFileTypeNames[DiffFileTypeCount] = {
    "funky thing",
    "block device",
    "character device",
    "directory",
    "file",
    "fifo",
    "symbolic link",
    "socket",
};

//
// ------------------------------------------------------------------ Functions
//

INT
DiffMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the diff utility.

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
    DIFF_CONTEXT Context;
    BOOL ContextLinesSpecified;
    PVOID NewBuffer;
    INT Option;
    BOOL OutputIsTerminal;
    int Status;

    memset(&Context, 0, sizeof(DIFF_CONTEXT));
    ContextLinesSpecified = FALSE;
    Context.OutputType = DiffOutputDefault;
    OutputIsTerminal = FALSE;
    if (isatty(STDOUT_FILENO) != 0) {
        OutputIsTerminal = TRUE;
        Context.Options |= DIFF_OPTION_COLOR;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             DIFF_OPTIONS_STRING,
                             DiffLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 2;
            goto MainEnd;
        }

        switch (Option) {
        case 'b':
            Context.Options |= DIFF_OPTION_IGNORE_BLANKS;
            break;

        case 'u':
            Context.OutputType = DiffOutputUnified;

            //
            // Fall through.
            //

        case 'c':
        case 'C':
            ContextLinesSpecified = TRUE;
            if (Context.OutputType == DiffOutputEd) {
                SwPrintError(0, NULL, "Conflicting output style options");
                Status = EINVAL;
                goto MainEnd;
            }

            Context.ContextLines = DIFF_DEFAULT_CONTEXT_LINES;
            if (optarg != NULL) {
                Context.ContextLines = strtol(optarg, &AfterScan, 10);
                if ((Context.ContextLines < 0) || (AfterScan == optarg)) {
                    SwPrintError(0, optarg, "Expected an integer");
                    Status = EINVAL;
                    goto MainEnd;
                }
            }

            break;

        case '1':

            assert(optarg != NULL);

            if (strcasecmp(optarg, "always") == 0) {
                Context.Options |= DIFF_OPTION_COLOR;

            } else if (strcasecmp(optarg, "never") == 0) {
                Context.Options &= ~DIFF_OPTION_COLOR;

            } else if (strcasecmp(optarg, "auto") == 0) {
                Context.Options &= ~DIFF_OPTION_COLOR;
                if (OutputIsTerminal != FALSE) {
                    Context.Options |= DIFF_OPTION_COLOR;
                }

            } else {
                SwPrintError(0, optarg, "Invalid color argument");
                Status = EINVAL;
                goto MainEnd;
            }

            break;

        case 'e':
            if (Context.ContextLines != 0) {
                SwPrintError(0, NULL, "Conflicting output style options");
                Status = EINVAL;
                goto MainEnd;
            }

            Context.OutputType = DiffOutputEd;
            break;

        case 'N':
            Context.Options |= DIFF_OPTION_ABSENT_EMPTY;
            break;

        case 'r':
            Context.Options |= DIFF_OPTION_RECURSIVE;
            break;

        case 'x':

            assert(optarg != NULL);

            NewBuffer = realloc(
                              Context.FileExclusions,
                              (Context.FileExclusionCount + 1) * sizeof(PSTR));

            if (NewBuffer == NULL) {
                Status = ENOMEM;
                goto MainEnd;
            }

            Context.FileExclusions = NewBuffer;
            Context.FileExclusions[Context.FileExclusionCount] = optarg;
            Context.FileExclusionCount += 1;
            break;

        case 'V':
            SwPrintVersion(DIFF_VERSION_MAJOR, DIFF_VERSION_MINOR);
            return 1;

        case 'h':
            printf(DIFF_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // If context was not specified and the format is still default, use the
    // ed format.
    //

    if ((Context.OutputType == DiffOutputDefault) &&
        (ContextLinesSpecified == FALSE)) {

        Context.OutputType = DiffOutputEd;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    //
    // Fail if there were not enough arguments.
    //

    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0,
                     NULL,
                     "Diff needs two things to compare. Try --help for usage");

        Status = 2;
        goto MainEnd;
    }

    if (ArgumentIndex + 2 != ArgumentCount) {
        SwPrintError(0,
                     NULL,
                     "Diff needs exactly two arguments. Try --help for usage");

        Status = 2;
        goto MainEnd;
    }

    Status = DiffComparePaths(&Context,
                              Arguments[ArgumentIndex],
                              Arguments[ArgumentIndex + 1]);

MainEnd:
    if (Context.FileExclusions != NULL) {
        free(Context.FileExclusions);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DiffComparePaths (
    PDIFF_CONTEXT Context,
    PSTR PathA,
    PSTR PathB
    )

/*++

Routine Description:

    This routine compares two paths, printing out the differences.

Arguments:

    Context - Supplies a pointer to the diff application context.

    PathA - Supplies a pointer to a null terminated string containing the first
        path to compare.

    PathB - Supplies a pointer to a null terminated string containing the
        second path to compare.

Return Value:

    0 if the files are equal.

    1 if there are differences.

    Other values on error.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    PSTR BaseName;
    PDIFF_FILE FileA;
    PDIFF_FILE FileB;
    INT Status;

    AppendedPath = NULL;
    FileA = NULL;
    FileB = NULL;
    Status = DiffCreateFile(Context, NULL, PathA, &FileA);
    if (Status != 0) {
        goto ComparePathsEnd;
    }

    Status = DiffCreateFile(Context, NULL, PathB, &FileB);
    if (Status != 0) {
        goto ComparePathsEnd;
    }

    //
    // If only one of A and B is a directory, then the diff is between the file
    // and directory/file (or basename of file).
    //

    if ((FileA->Type == DiffFileDirectory) &&
        (FileB->Type != DiffFileDirectory)) {

        BaseName = basename(PathB);
        Status = SwAppendPath(PathA,
                              strlen(PathA) + 1,
                              BaseName,
                              strlen(BaseName) + 1,
                              &AppendedPath,
                              &AppendedPathSize);

        if (Status != 0) {
            SwPrintError(Status, NULL, "Could not append paths");
            goto ComparePathsEnd;
        }

        DiffDestroyFile(Context, FileB);
        Status = DiffCreateFile(Context, NULL, AppendedPath, &FileB);
        if (Status != 0) {
            goto ComparePathsEnd;
        }

    } else if ((FileA->Type != DiffFileDirectory) &&
               (FileB->Type == DiffFileDirectory)) {

        BaseName = basename(PathA);
        Status = SwAppendPath(PathB,
                              strlen(PathB) + 1,
                              BaseName,
                              strlen(BaseName) + 1,
                              &AppendedPath,
                              &AppendedPathSize);

        if (Status != 0) {
            SwPrintError(Status, NULL, "Could not append paths");
            goto ComparePathsEnd;
        }

        DiffDestroyFile(Context, FileA);
        Status = DiffCreateFile(Context, NULL, AppendedPath, &FileA);
        if (Status != 0) {
            goto ComparePathsEnd;
        }
    }

    Status = DiffCompareFiles(Context, NULL, FileA, NULL, FileB, 0);

ComparePathsEnd:
    if (AppendedPath != NULL) {
        free(AppendedPath);
    }

    if (FileA != NULL) {
        DiffDestroyFile(Context, FileA);
    }

    if (FileB != NULL) {
        DiffDestroyFile(Context, FileB);
    }

    return Status;
}

INT
DiffCompareFiles (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB,
    ULONG RecursionLevel
    )

/*++

Routine Description:

    This routine compares two file structures, printing out the differences.

Arguments:

    Context - Supplies a pointer to the diff application context.

    DirectoryA - Supplies a pointer to the directory prefix for file A.

    FileA - Supplies a pointer to the first file object to compare.

    DirectoryB - Supplies a pointer to the directory prefix for file B.

    FileB - Supplies a pointer to the second file object to compare.

    RecursionLevel - Supplies the level of recursion this function is operating
        in.

Return Value:

    0 if the files are equal.

    1 if there are differences.

    Other values on error.

--*/

{

    PSTR AppendedPathA;
    ULONG AppendedPathASize;
    PSTR AppendedPathB;
    ULONG AppendedPathBSize;
    ULONG DirectorySize;
    INT Status;

    AppendedPathA = NULL;
    AppendedPathB = NULL;

    //
    // If the types are not equal, simply report that.
    //

    if ((DirectoryA != NULL) && (DirectoryB != NULL)) {
        if (FileA->Type != FileB->Type) {
            printf("File %s is a %s while file %s is a %s.\n",
                   FileA->Name,
                   DiffFileTypeNames[FileA->Type],
                   FileB->Name,
                   DiffFileTypeNames[FileB->Type]);

            Status = 1;
            goto CompareFilesEnd;
        }
    }

    Status = 0;

    //
    // The file types are equal. If they're files or directories, compare them.
    //

    if (FileA->Type != DiffFileDirectory) {
        Status = DiffCompareRegularFiles(Context,
                                         DirectoryA,
                                         FileA,
                                         DirectoryB,
                                         FileB,
                                         RecursionLevel);

    } else {

        //
        // Compare the contents of the directories if either 1) this is the
        // entry directly from the command line or 2) The recursion option is
        // enabled and the current recursion level is below the maximum
        // depth.
        //

        if ((RecursionLevel == 0) ||
            ((Context->Options & DIFF_OPTION_RECURSIVE) != 0)) {

            if (RecursionLevel >= DIFF_MAX_RECURSION_DEPTH) {
                SwPrintError(0, FileA->Name, "Max recursion depth reached");
                Status = ELOOP;
                goto CompareFilesEnd;
            }

            DirectorySize = 0;
            if (DirectoryA != NULL) {
                DirectorySize = strlen(DirectoryA) + 1;
            }

            Status = SwAppendPath(DirectoryA,
                                  DirectorySize,
                                  FileA->Name,
                                  strlen(FileA->Name) + 1,
                                  &AppendedPathA,
                                  &AppendedPathASize);

            if (Status == FALSE) {
                Status = ENOMEM;
                goto CompareFilesEnd;
            }

            DirectorySize = 0;
            if (DirectoryB != NULL) {
                DirectorySize = strlen(DirectoryB) + 1;
            }

            Status = SwAppendPath(DirectoryB,
                                  DirectorySize,
                                  FileB->Name,
                                  strlen(FileB->Name) + 1,
                                  &AppendedPathB,
                                  &AppendedPathBSize);

            if (Status == FALSE) {
                Status = ENOMEM;
                goto CompareFilesEnd;
            }

            Status = DiffCompareDirectories(Context,
                                            AppendedPathA,
                                            AppendedPathB,
                                            RecursionLevel);
        }
    }

CompareFilesEnd:
    if (AppendedPathA != NULL) {
        free(AppendedPathA);
    }

    if (AppendedPathB != NULL) {
        free(AppendedPathB);
    }

    return Status;
}

INT
DiffCompareDirectories (
    PDIFF_CONTEXT Context,
    PSTR PathA,
    PSTR PathB,
    ULONG RecursionLevel
    )

/*++

Routine Description:

    This routine compares the contents of two directories, printing out the
    differences.

Arguments:

    Context - Supplies a pointer to the diff application context.

    PathA - Supplies a pointer to a null terminated string containing the first
        directory to compare.

    PathB - Supplies a pointer to a null terminated string containing the
        directory path to compare.

    RecursionLevel - Supplies the current recursion depth.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PDIFF_DIRECTORY DirectoryA;
    PDIFF_DIRECTORY DirectoryB;
    PDIFF_FILE FileA;
    UINTN FileAIndex;
    PDIFF_FILE FileB;
    UINTN FileBIndex;
    INT NameComparison;
    INT Status;
    INT TotalStatus;

    TotalStatus = 0;

    //
    // Enumerate both directories.
    //

    DirectoryA = NULL;
    DirectoryB = NULL;
    Status = DiffGetDirectoryListing(Context, PathA, &DirectoryA);
    if (Status != 0) {
        SwPrintError(Status, PathA, "Unable to enumerate directory");
        goto DiffCompareDirectoriesEnd;
    }

    Status = DiffGetDirectoryListing(Context, PathB, &DirectoryB);
    if (Status != 0) {
        SwPrintError(Status, PathB, "Unable to enumerate directory");
        goto DiffCompareDirectoriesEnd;
    }

    //
    // Loop through until all files have been dealt with.
    //

    FileAIndex = 0;
    FileBIndex = 0;
    while ((FileAIndex < DirectoryA->FileCount) ||
           (FileBIndex < DirectoryB->FileCount)) {

        FileA = NULL;
        FileB = NULL;

        //
        // If A is at the end, then just print that B exists.
        //

        if ((FileAIndex == DirectoryA->FileCount) &&
            ((Context->Options & DIFF_OPTION_ABSENT_EMPTY) == 0)) {

            assert(FileBIndex < DirectoryB->FileCount);

            FileB = DirectoryB->Files[FileBIndex];
            printf("Only in %s: %s\n", PathB, FileB->Name);
            FileBIndex += 1;
            if (TotalStatus == 0) {
                TotalStatus = 1;
            }

        //
        // If B is at the end, then just print that A exists.
        //

        } else if ((FileBIndex == DirectoryB->FileCount) &&
                   ((Context->Options & DIFF_OPTION_ABSENT_EMPTY) == 0)) {

            assert(FileAIndex < DirectoryA->FileCount);

            FileA = DirectoryA->Files[FileAIndex];
            printf("Only in %s: %s\n", PathA, FileA->Name);
            FileAIndex += 1;
            if (TotalStatus == 0) {
                TotalStatus = 1;
            }

        //
        // They both exist (or absent files are treated as empty.
        //

        } else {
            if (FileAIndex < DirectoryA->FileCount) {
                FileA = DirectoryA->Files[FileAIndex];
            }

            if (FileBIndex < DirectoryB->FileCount) {
                FileB = DirectoryB->Files[FileBIndex];
            }

            //
            // Use the empty file if either file came up NULL.
            //

            if (FileA == NULL) {
                FileA = &(Context->EmptyFile);
                FileA->Type = FileB->Type;
                FileA->Name = FileB->Name;

            } else if (FileB == NULL) {
                FileB = &(Context->EmptyFile);
                FileB->Type = FileA->Type;
                FileB->Name = FileA->Name;
            }

            //
            // They both exist. If they're the same file, then actually compare
            // them.
            //

            NameComparison = strcmp(FileA->Name, FileB->Name);
            if (NameComparison == 0) {
                if ((FileA->Type == DiffFileDirectory) &&
                    (FileB->Type == DiffFileDirectory) &&
                    ((Context->Options & DIFF_OPTION_RECURSIVE) == 0)) {

                    printf("Common subdirectories: %s/%s and %s/%s\n",
                           PathA,
                           FileA->Name,
                           PathB,
                           FileB->Name);
                }

                Status = DiffCompareFiles(Context,
                                          PathA,
                                          FileA,
                                          PathB,
                                          FileB,
                                          RecursionLevel + 1);

                if (Status != 0) {
                    if (TotalStatus == 0) {
                        TotalStatus = Status;
                    }
                }

                //
                // Destroy files now to conserve memory and file descriptors.
                //

                DiffDestroyFile(Context, FileA);
                if (FileAIndex < DirectoryA->FileCount) {
                    DirectoryA->Files[FileAIndex] = NULL;
                }

                DiffDestroyFile(Context, FileB);
                if (FileBIndex < DirectoryB->FileCount) {
                    DirectoryB->Files[FileBIndex] = NULL;
                }

                FileAIndex += 1;
                FileBIndex += 1;

            //
            // Name whichever file is lower as the oddball in the directory.
            //

            } else if (NameComparison < 0) {
                if ((Context->Options & DIFF_OPTION_ABSENT_EMPTY) != 0) {
                    FileB = &(Context->EmptyFile);
                    FileB->Type = FileA->Type;
                    FileB->Name = FileA->Name;
                    Status = DiffCompareFiles(Context,
                                              PathA,
                                              FileA,
                                              PathB,
                                              FileB,
                                              RecursionLevel + 1);

                    if (Status != 0) {
                        if (TotalStatus == 0) {
                            TotalStatus = Status;
                        }
                    }

                } else {
                    printf("Only in %s: %s\n", PathA, FileA->Name);
                }

                DiffDestroyFile(Context, FileA);
                DirectoryA->Files[FileAIndex] = NULL;
                FileAIndex += 1;

            } else {
                if ((Context->Options & DIFF_OPTION_ABSENT_EMPTY) != 0) {
                    FileA = &(Context->EmptyFile);
                    FileA->Type = FileB->Type;
                    FileA->Name = FileB->Name;
                    Status = DiffCompareFiles(Context,
                                              PathA,
                                              FileA,
                                              PathB,
                                              FileB,
                                              RecursionLevel + 1);

                    if (Status != 0) {
                        if (TotalStatus == 0) {
                            TotalStatus = Status;
                        }
                    }

                } else {
                    printf("Only in %s: %s\n", PathB, FileB->Name);
                }

                DiffDestroyFile(Context, FileB);
                DirectoryB->Files[FileBIndex] = NULL;
                FileBIndex += 1;
            }
        }
    }

DiffCompareDirectoriesEnd:
    if (DirectoryA != NULL) {
        DiffDestroyDirectory(Context, DirectoryA);
    }

    if (DirectoryB != NULL) {
        DiffDestroyDirectory(Context, DirectoryB);
    }

    if (Status != 0) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

INT
DiffGetDirectoryListing (
    PDIFF_CONTEXT Context,
    PSTR Path,
    PDIFF_DIRECTORY *NewDirectory
    )

/*++

Routine Description:

    This routine enumerates the contents of a directory.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the null terminated path of the directory to
        enumerate.

    NewDirectory - Supplies a pointer where a pointer to the new directory
        structure will be returned on success.

Return Value:

    0 on success.

    Returns a non-zero error code on failure.

--*/

{

    PDIFF_DIRECTORY Directory;
    DIR *DirectoryFile;
    struct dirent *Entry;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PDIFF_FILE NewFile;
    INT Status;

    NewFile = NULL;
    Directory = NULL;
    DirectoryFile = opendir(Path);
    if ((DirectoryFile == NULL) && (errno != ENOENT)) {
        Status = errno;
        SwPrintError(Status, Path, "Failed to open directory");
        goto GetDirectoryListingEnd;
    }

    Directory = malloc(sizeof(DIFF_DIRECTORY));
    if (Directory == NULL) {
        Status = ENOMEM;
        goto GetDirectoryListingEnd;
    }

    memset(Directory, 0, sizeof(DIFF_DIRECTORY));
    if (DirectoryFile == NULL) {
        Status = 0;
        goto GetDirectoryListingEnd;
    }

    while (TRUE) {
        errno = 0;
        Entry = readdir(DirectoryFile);
        if (Entry == NULL) {
            Status = errno;
            if (Status != 0) {
                SwPrintError(Status, Path, "Unable to read directory");
                goto GetDirectoryListingEnd;
            }

            break;
        }

        if ((strcmp(Entry->d_name, ".") == 0) ||
            (strcmp(Entry->d_name, "..") == 0)) {

            continue;
        }

        //
        // Skip the file if it's excluded.
        //

        if (DiffIsFileNameExcluded(Context, Entry->d_name) != 0) {
            continue;
        }

        //
        // Expand the array if there's not space.
        //

        if (Directory->FileCount == Directory->ArrayCapacity) {
            NewCapacity = Directory->ArrayCapacity * 2;
            if (NewCapacity == 0) {
                NewCapacity = DIFF_INITIAL_ARRAY_CAPACITY;
            }

            NewBuffer = realloc(Directory->Files,
                                NewCapacity * sizeof(PDIFF_FILE));

            if (NewBuffer == NULL) {
                Status = ENOMEM;
                goto GetDirectoryListingEnd;
            }

            Directory->Files = NewBuffer;
            Directory->ArrayCapacity = NewCapacity;
        }

        //
        // Create the file and add it to the array.
        //

        Status = DiffCreateFile(Context, Path, Entry->d_name, &NewFile);
        if (Status != 0) {
            goto GetDirectoryListingEnd;
        }

        Directory->Files[Directory->FileCount] = NewFile;
        Directory->FileCount += 1;
        NewFile = NULL;
    }

    //
    // Sort the files.
    //

    qsort(Directory->Files,
          Directory->FileCount,
          sizeof(PDIFF_FILE),
          DiffCompareFileNames);

GetDirectoryListingEnd:
    if (DirectoryFile != NULL) {
        closedir(DirectoryFile);
    }

    if (NewFile != NULL) {
        DiffDestroyFile(Context, NewFile);
    }

    if (Status != 0) {
        if (Directory != NULL) {
            DiffDestroyDirectory(Context, Directory);
            Directory = NULL;
        }
    }

    *NewDirectory = Directory;
    return Status;
}

BOOL
DiffIsFileNameExcluded (
    PDIFF_CONTEXT Context,
    PSTR Name
    )

/*++

Routine Description:

    This routine determines if a file name should be excluded because it
    matches one of the specified exclusion patterns.

Arguments:

    Context - Supplies a pointer to the application context.

    Name - Supplies a pointer to the name to compare.

Return Value:

    TRUE if the file is excluded.

    FALSE if the file is included.

--*/

{

    UINTN ExclusionIndex;
    BOOL Match;
    UINTN NameSize;
    PSTR Pattern;
    UINTN PatternSize;

    if (Context->FileExclusionCount == 0) {
        return FALSE;
    }

    NameSize = strlen(Name) + 1;
    for (ExclusionIndex = 0;
         ExclusionIndex < Context->FileExclusionCount;
         ExclusionIndex += 1) {

        Pattern = Context->FileExclusions[ExclusionIndex];
        PatternSize = strlen(Pattern) + 1;
        Match = SwDoesPathPatternMatch(Name, NameSize, Pattern, PatternSize);
        if (Match != FALSE) {
            return TRUE;
        }
    }

    return FALSE;
}

VOID
DiffDestroyDirectory (
    PDIFF_CONTEXT Context,
    PDIFF_DIRECTORY Directory
    )

/*++

Routine Description:

    This routine destroys a diff directory structure.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies a pointer to the directory structure to destroy.

Return Value:

    None.

--*/

{

    PDIFF_FILE File;
    UINTN Index;

    for (Index = 0; Index < Directory->FileCount; Index += 1) {
        File = Directory->Files[Index];
        if (File != NULL) {
            DiffDestroyFile(Context, File);
        }
    }

    if (Directory->Files != NULL) {
        free(Directory->Files);
    }

    free(Directory);
    return;
}

INT
DiffCreateFile (
    PDIFF_CONTEXT Context,
    PSTR Directory,
    PSTR Path,
    PDIFF_FILE *File
    )

/*++

Routine Description:

    This routine creates a diff file structure based on the given path.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies an optional pointer to a directory prefix of the path.

    Path - Supplies a pointer to the null terminated path of the file to
        create an entry for.

    File - Supplies a pointer where a pointer to the new file structure will be
        returned on success.

Return Value:

    0 on success.

    Returns a non-zero error code on failure.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    ULONG DirectorySize;
    PDIFF_FILE NewFile;
    struct stat Stat;
    INT Status;

    AppendedPath = NULL;
    NewFile = malloc(sizeof(DIFF_FILE));
    if (NewFile == NULL) {
        Status = ENOMEM;
        goto CreateFileEnd;
    }

    memset(NewFile, 0, sizeof(DIFF_FILE));
    NewFile->Name = strdup(Path);
    if (NewFile->Name == NULL) {
        Status = ENOMEM;
        goto CreateFileEnd;
    }

    if ((Directory == NULL) && (strcmp(Path, "-") == 0)) {
        NewFile->File = stdin;
        NewFile->ModificationTime = time(NULL);
        NewFile->Type = DiffFileRegularFile;

    } else {
        DirectorySize = 0;
        if (Directory != NULL) {
            DirectorySize = strlen(Directory) + 1;
        }

        Status = SwAppendPath(Directory,
                              DirectorySize,
                              Path,
                              strlen(Path) + 1,
                              &AppendedPath,
                              &AppendedPathSize);

        if (Status == FALSE) {
            Status = ENOMEM;
            goto CreateFileEnd;
        }

        Status = SwStat(AppendedPath, TRUE, &Stat);
        if (Status != 0) {
            SwPrintError(errno, AppendedPath, "Unable to stat");
            Status = errno;
            goto CreateFileEnd;
        }

        NewFile->ModificationTime = Stat.st_mtime;
        NewFile->Type = DiffGetFileType(Stat.st_mode);
    }

    Status = 0;

CreateFileEnd:
    if (AppendedPath != NULL) {
        free(AppendedPath);
    }

    if (Status != 0) {
        if (NewFile != NULL) {
            DiffDestroyFile(Context, NewFile);
            NewFile = NULL;
        }
    }

    *File = NewFile;
    return Status;
}

VOID
DiffDestroyFile (
    PDIFF_CONTEXT Context,
    PDIFF_FILE File
    )

/*++

Routine Description:

    This routine destroys a diff file structure.

Arguments:

    Context - Supplies the application context.

    File - Supplies a pointer to the file to destroy.

Return Value:

    None.

--*/

{

    UINTN LineIndex;

    if (File == &(Context->EmptyFile)) {
        File->Name = NULL;
        return;
    }

    if (File->Name != NULL) {
        free(File->Name);
    }

    if ((File->File != NULL) && (File->File != stdin)) {
        fclose(File->File);
    }

    for (LineIndex = 0; LineIndex < File->LineCount; LineIndex += 1) {
        DiffDestroyLine(File->Lines[LineIndex]);
    }

    if (File->Lines != NULL) {
        free(File->Lines);
    }

    free(File);
    return;
}

VOID
DiffDestroyLine (
    PDIFF_LINE Line
    )

/*++

Routine Description:

    This routine destroys a diff line structure.

Arguments:

    Line - Supplies a pointer to the line to destroy.

Return Value:

    None.

--*/

{

    if (Line->Data != NULL) {
        free(Line->Data);
    }

    free(Line);
    return;
}

DIFF_FILE_TYPE
DiffGetFileType (
    mode_t Mode
    )

/*++

Routine Description:

    This routine returns the diff file type for the given mode.

Arguments:

    Mode - Supplies the mode bits coming back from stat.

Return Value:

    Returns the diff file type.

--*/

{

    if (S_ISBLK(Mode)) {
        return DiffFileBlockDevice;

    } else if (S_ISCHR(Mode)) {
        return DiffFileCharacterDevice;

    } else if (S_ISDIR(Mode)) {
        return DiffFileDirectory;

    } else if (S_ISREG(Mode)) {
        return DiffFileRegularFile;

    } else if (S_ISFIFO(Mode)) {
        return DiffFileFifo;

    } else if (S_ISLNK(Mode)) {
        return DiffFileLink;

    } else if (S_ISSOCK(Mode)) {
        return DiffFileSocket;
    }

    return DiffFileUnknown;
}

int
DiffCompareFileNames (
    const void *LeftPointer,
    const void *RightPointer
    )

/*++

Routine Description:

    This routine compares the names of two diff files.

Arguments:

    LeftPointer - Supplies a pointer to a pointer to the left diff file of the
        comparison.

    RightPointer - Supplies a pointer to a pointer to the right diff file of
        the comparison.

Return Value:

    <0 if the left is less than the right.

    0 if the two are equal.

    >0 if the left is greater than the right.

--*/

{

    PDIFF_FILE LeftFile;
    PDIFF_FILE RightFile;

    LeftFile = *((PDIFF_FILE *)LeftPointer);
    RightFile = *((PDIFF_FILE *)RightPointer);
    return strcmp(LeftFile->Name, RightFile->Name);
}

VOID
DiffPrintCommandLine (
    PDIFF_CONTEXT Context,
    PSTR PathA,
    PDIFF_FILE FileA,
    PSTR PathB,
    PDIFF_FILE FileB
    )

/*++

Routine Description:

    This routine prints the diff command line corresponding to the given
    context and files.

Arguments:

    Context - Supplies a pointer to the application context.

    PathA - Supplies the directory path of the left file.

    FileA - Supplies a pointer to the left file information.

    PathB - Supplies the directory path of the right file.

    FileB - Supplies a pointer to the right file information.

Return Value:

    None.

--*/

{

    printf("diff ");
    if ((Context->Options & DIFF_OPTION_IGNORE_BLANKS) != 0) {
        printf("-b ");
    }

    if ((Context->Options & DIFF_OPTION_RECURSIVE) != 0) {
        printf("-r ");
    }

    if ((Context->Options & DIFF_OPTION_ABSENT_EMPTY) != 0) {
        printf("-N ");
    }

    if (Context->OutputType == DiffOutputUnified) {
        printf("-u %d ", Context->ContextLines);

    } else if (Context->OutputType == DiffOutputEd) {
        printf("-e ");

    } else if (Context->ContextLines != 0) {
        printf("-C %d ", Context->ContextLines);
    }

    printf("%s/%s %s/%s\n", PathA, FileA->Name, PathB, FileB->Name);
    return;
}

INT
DiffLoadFile (
    PDIFF_CONTEXT Context,
    PSTR Directory,
    PDIFF_FILE File
    )

/*++

Routine Description:

    This routine loads the contents of a file into lines.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies an optional pointer to a null terminated string
        containing the directory the file is in.

    File - Supplies a pointer to the file to load.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    INT Character;
    ULONG DirectorySize;
    PDIFF_LINE Line;
    UINTN LineArrayCapacity;
    PSTR LineBuffer;
    UINTN LineBufferCapacity;
    UINTN LineBufferSize;
    ULONG LineHash;
    PVOID NewBuffer;
    UINTN NewCapacity;
    INT Status;

    AppendedPath = NULL;
    Line = NULL;
    LineArrayCapacity = 0;
    LineBuffer = NULL;
    LineBufferCapacity = 0;
    LineBufferSize = 0;
    if (File == &(Context->EmptyFile)) {
        Status = 0;
        goto LoadFileEnd;
    }

    if (File->File == NULL) {
        DirectorySize = 0;
        if (Directory != NULL) {
            DirectorySize = strlen(Directory) + 1;
        }

        Status = SwAppendPath(Directory,
                              DirectorySize,
                              File->Name,
                              strlen(File->Name) + 1,
                              &AppendedPath,
                              &AppendedPathSize);

        if (Status == FALSE) {
            Status = ENOMEM;
            goto LoadFileEnd;
        }

        File->File = fopen(AppendedPath, "r");
        if (File->File == NULL) {
            Status = errno;
            SwPrintError(Status, AppendedPath, "Failed to open");
            goto LoadFileEnd;
        }
    }

    Status = ENOMEM;

    //
    // Loop loading lines.
    //

    LineBufferCapacity = DIFF_INITIAL_LINE_BUFFER;
    LineBuffer = malloc(LineBufferCapacity);
    if (LineBuffer == NULL) {
        goto LoadFileEnd;
    }

    while (TRUE) {
        LineBufferSize = 0;
        LineHash = 0;

        //
        // Loop adding characters to the line.
        //

        while (TRUE) {
            Character = fgetc(File->File);
            if ((Character == EOF) || (Character == '\n')) {
                break;
            }

            if (Character == '\0') {
                File->Binary = TRUE;
                break;
            }

            //
            // Expand the line buffer if it's not big enough to hold the
            // character.
            //

            if (LineBufferSize + 1 >= LineBufferCapacity) {
                NewCapacity = LineBufferCapacity * 2;

                assert(NewCapacity > LineBufferSize + 1);

                NewBuffer = realloc(LineBuffer, NewCapacity);
                if (NewBuffer == NULL) {
                    goto LoadFileEnd;
                }

                LineBuffer = NewBuffer;
                LineBufferCapacity = NewCapacity;
            }

            //
            // Add the character to the line buffer.
            //

            LineBuffer[LineBufferSize] = (CHAR)Character;
            LineBufferSize += 1;

            //
            // The poor man's hash is really just the sum of all the
            // characters. Don't count this character as part of the hash if
            // it's blank and blanks are being ignored.
            //

            if (((Context->Options & DIFF_OPTION_IGNORE_BLANKS) == 0) ||
                (isspace(Character) == 0)) {

                LineHash += Character;
            }
        }

        //
        // If the file ended with a newline, that's normal, just stop before
        // adding another empty line. The newline really belongs to the the
        // previous line.
        //

        if ((Character == EOF) && (LineBufferSize == 0)) {
            break;
        }

        //
        // Allocate a line structure.
        //

        Line = malloc(sizeof(DIFF_LINE));
        if (Line == NULL) {
            goto LoadFileEnd;
        }

        memset(Line, 0, sizeof(DIFF_LINE));

        //
        // Allocate a line of exactly the right size and copy it in.
        //

        Line->Data = malloc(LineBufferSize + 1);
        if (Line->Data == NULL) {
            goto LoadFileEnd;
        }

        memcpy(Line->Data, LineBuffer, LineBufferSize);
        Line->Data[LineBufferSize] = '\0';
        Line->Size = LineBufferSize + 1;
        Line->Hash = LineHash;

        //
        // Expand the line array if it is not big enough to hold this line.
        //

        if (File->LineCount >= LineArrayCapacity) {
            NewCapacity = LineArrayCapacity * 2;
            if (NewCapacity == 0) {
                NewCapacity = DIFF_INITIAL_ARRAY_CAPACITY;
            }

            assert(NewCapacity > File->LineCount);

            NewBuffer = realloc(File->Lines, NewCapacity * sizeof(PDIFF_LINE));
            if (NewBuffer == NULL) {
                goto LoadFileEnd;
            }

            File->Lines = NewBuffer;
            LineArrayCapacity = NewCapacity;
        }

        File->Lines[File->LineCount] = Line;
        File->LineCount += 1;
        Line = NULL;

        //
        // If this was the last line, stop.
        //

        if (Character == EOF) {
            File->NoNewlineAtEnd = TRUE;
            break;
        }
    }

    //
    // If the file has a problem, fail.
    //

    if (ferror(File->File) != 0) {
        Status = errno;
        SwPrintError(Status, File->Name, "Failed to read");
        goto LoadFileEnd;
    }

    Status = 0;

LoadFileEnd:
    if (Line != NULL) {
        DiffDestroyLine(Line);
    }

    if (AppendedPath != NULL) {
        free(AppendedPath);
    }

    if (LineBuffer != NULL) {
        free(LineBuffer);
    }

    return Status;
}

//
// Routines for computing the difference between two files
//

INT
DiffCompareRegularFiles (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB,
    ULONG RecursionLevel
    )

/*++

Routine Description:

    This routine compares two regular files, printing out the difference.

Arguments:

    Context - Supplies a pointer to the diff application context.

    DirectoryA - Supplies a pointer to the directory prefix for file A.

    FileA - Supplies a pointer to the first file object to compare.

    DirectoryB - Supplies a pointer to the directory prefix for file B.

    FileB - Supplies a pointer to the second file object to compare.

    RecursionLevel - Supplies the level of recursion this function is operating
        in.

Return Value:

    0 if the files are equal.

    1 if there are differences.

    Other values on error.

--*/

{

    PINTN DownVector;
    UINTN Maximum;
    INT Status;
    PINTN UpVector;
    UINTN VectorSize;

    //
    // Load up the two files.
    //

    Status = DiffLoadFile(Context, DirectoryA, FileA);
    if (Status != 0) {
        SwPrintError(Status,
                     NULL,
                     "Failed to load file '%s/%s'",
                     DirectoryA,
                     FileA->Name);

        goto CompareRegularFilesEnd;
    }

    Status = DiffLoadFile(Context, DirectoryB, FileB);
    if (Status != 0) {
        SwPrintError(Status,
                     NULL,
                     "Failed to load file '%s/%s'",
                     DirectoryB,
                     FileB->Name);

        goto CompareRegularFilesEnd;
    }

    //
    // If either file is binary, just perform a binary comparison and report
    // whether or not they're the same.
    //

    if ((FileA->Binary != FALSE) || (FileB->Binary != FALSE)) {
        Status = DiffCompareBinaryFiles(Context, FileA, FileB);
        if (Status == 1) {
            if (RecursionLevel != 0) {
                DiffPrintCommandLine(Context,
                                     DirectoryA,
                                     FileA,
                                     DirectoryB,
                                     FileB);
            }

            printf("Binary files '");
            if (DirectoryA != NULL) {
                printf("%s/%s", DirectoryA, FileA->Name);

            } else {
                printf("%s", FileA->Name);
            }

            printf("' and '");
            if (DirectoryB != NULL) {
                printf("%s/%s", DirectoryB, FileB->Name);

            } else {
                printf("%s", FileB->Name);
            }

            printf("' differ.\n");
        }

        return Status;
    }

    //
    // Allocate vectors (V in the paper) for computing the shortest middle
    // snake from both directions (forward and reverse). The vectors are
    // indexed by k-line, which is the distance from the diagonal. The maximum
    // possible distance is the sum of the two lengths. This goes in either
    // direction (times two), plus two extra.
    //

    Maximum = FileA->LineCount + FileB->LineCount + 1;
    VectorSize = (2 * Maximum) + 2;
    DownVector = malloc(sizeof(INTN) * VectorSize);
    if (DownVector == NULL) {
        Status = ENOMEM;
        goto CompareRegularFilesEnd;
    }

    UpVector = malloc(sizeof(INTN) * VectorSize);
    if (UpVector == NULL) {
        Status = ENOMEM;
        goto CompareRegularFilesEnd;
    }

    //
    // Find the longest common subsequence, which marks the different lines
    // as modified.
    //

    Status = DiffComputeLongestCommonSubsequence(Context,
                                                 FileA,
                                                 FileB,
                                                 0,
                                                 FileA->LineCount,
                                                 0,
                                                 FileB->LineCount,
                                                 DownVector,
                                                 UpVector);

    if (Status != 1) {
        goto CompareRegularFilesEnd;
    }

    if (RecursionLevel != 0) {
        DiffPrintCommandLine(Context, DirectoryA, FileA, DirectoryB, FileB);
    }

    //
    // Print the diffs in the desired format.
    //

    switch (Context->OutputType) {
    case DiffOutputDefault:
        DiffPrintContextDiffs(Context, DirectoryA, FileA, DirectoryB, FileB);
        break;

    case DiffOutputEd:
        DiffPrintEdDiffs(Context, DirectoryA, FileA, DirectoryB, FileB);
        break;

    case DiffOutputUnified:
        DiffPrintUnifiedDiffs(Context, DirectoryA, FileA, DirectoryB, FileB);
        break;

    default:

        assert(FALSE);

        Status = EINVAL;
        goto CompareRegularFilesEnd;
    }

CompareRegularFilesEnd:
    return Status;
}

INT
DiffCompareBinaryFiles (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB
    )

/*++

Routine Description:

    This routine compares two binary files for equality. The ignore blanks
    flag is ignored here.

Arguments:

    Context - Supplies a pointer to the diff application context.

    FileA - Supplies a pointer to the first file.

    FileB - Supplies a pointer to the second file.

Return Value:

    0 if the files are equal in the compared region.

    1 if the files differ somewhere.

    Other codes on error.

--*/

{

    INT CharacterA;
    INT CharacterB;
    INT Status;

    //
    // If one file is not there but the other is, they're different. This
    // happens when non-existant files are treated as empty.
    //

    if (((FileA->File == NULL) && (FileB->File != NULL)) ||
        ((FileA->File != NULL) && (FileB->File == NULL))) {

        Status = 1;
        goto CompareBinaryFilesEnd;
    }

    assert((FileA->File != NULL) && (FileB->File != NULL));

    rewind(FileA->File);
    rewind(FileB->File);
    Status = 0;
    while (TRUE) {
        CharacterA = fgetc(FileA->File);
        if (ferror(FileA->File)) {
            Status = errno;
            SwPrintError(Status, FileA->Name, "Failed to read");
            goto CompareBinaryFilesEnd;
        }

        CharacterB = fgetc(FileB->File);
        if (ferror(FileB->File)) {
            Status = errno;
            SwPrintError(Status, FileB->Name, "Failed to read");
            goto CompareBinaryFilesEnd;
        }

        if (CharacterA != CharacterB) {
            Status = 1;
            goto CompareBinaryFilesEnd;
        }

        if (CharacterA == EOF) {
            break;
        }
    }

CompareBinaryFilesEnd:
    return Status;
}

INT
DiffComputeLongestCommonSubsequence (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    INTN LowerA,
    INTN UpperA,
    INTN LowerB,
    INTN UpperB,
    PINTN DownVector,
    PINTN UpVector
    )

/*++

Routine Description:

    This routine implements the Myers' algorithm for computing the longest
    common subsequence in linear space (but with recursion). The algorithm is
    a divide-and-conquer algorithm, finding an element of the correct path
    in the middle and then recursing on each of the slightly smaller split
    pieces.

Arguments:

    Context - Supplies a pointer to the diff application context.

    FileA - Supplies a pointer to the first file.

    FileB - Supplies a pointer to the second file.

    LowerA - Supplies the starting index within file A to work on.

    UpperA - Supplies the ending index within file A to work on, exclusive.

    LowerB - Supplies the starting index within file B to work on.

    UpperB - Supplies the ending index within file B to work on, exclusive.

    DownVector - Supplies the k-indexed vector for computing the shortest
        middle snake from the top down.

    UpVector - Supplies the k-indexed vector for computing the shortest middle
        snake from the bottom up.

Return Value:

    0 if the files are equal in the compared region.

    1 if the files differ somewhere.

--*/

{

    INTN MiddleSnakeX;
    INTN MiddleSnakeY;
    INT Status;
    INT TotalStatus;

    Status = 0;
    TotalStatus = 0;

    //
    // As a basic no-brainer, get past any lines at the beginning and the
    // end that match.
    //

    while ((LowerA < UpperA) && (LowerB < UpperB)) {
        Status = DiffCompareLines(Context, FileA, FileB, LowerA, LowerB);
        if (Status != 0) {
            break;
        }

        LowerA += 1;
        LowerB += 1;
    }

    if (Status != 0) {
        TotalStatus = Status;
    }

    while ((LowerA < UpperA) && (LowerB < UpperB)) {
        Status = DiffCompareLines(Context,
                                  FileA,
                                  FileB,
                                  UpperA - 1,
                                  UpperB - 1);

        if (Status != 0) {
            break;
        }

        UpperA -= 1;
        UpperB -= 1;
    }

    if (Status != 0) {
        TotalStatus = Status;
    }

    //
    // If file A ended, then mark everything in file B as an insertion.
    //

    if (LowerA == UpperA) {
        if (LowerB < UpperB) {
            TotalStatus = 1;
        }

        while (LowerB < UpperB) {
            FileB->Lines[LowerB]->Modified = TRUE;
            LowerB += 1;
        }

    //
    // If file B ended, then mark everything in file A as a deletion.
    //

    } else if (LowerB == UpperB) {
        if (LowerA < UpperA) {
            TotalStatus = 1;
        }

        while (LowerA < UpperA) {
            FileA->Lines[LowerA]->Modified = TRUE;
            LowerA += 1;
        }

    //
    // Run the real crux of the diff algorithm.
    //

    } else {

        //
        // Find the shortest middle snake, which returns a k index into the
        // down vector array. This index contains the x value of the shortest
        // middle snake. The y value is then x - k.
        //

        DiffComputeShortestMiddleSnake(Context,
                                       FileA,
                                       FileB,
                                       LowerA,
                                       UpperA,
                                       LowerB,
                                       UpperB,
                                       DownVector,
                                       UpVector,
                                       &MiddleSnakeX,
                                       &MiddleSnakeY);

        //
        // Now that a middle value in the longest common subsequence is known,
        // recurse down to find the longest common subsequences of the upper
        // left box and lower right box that remains.
        //

        Status = DiffComputeLongestCommonSubsequence(Context,
                                                     FileA,
                                                     FileB,
                                                     LowerA,
                                                     MiddleSnakeX,
                                                     LowerB,
                                                     MiddleSnakeY,
                                                     DownVector,
                                                     UpVector);

        if (Status != 0) {
            TotalStatus = Status;
        }

        Status = DiffComputeLongestCommonSubsequence(Context,
                                                     FileA,
                                                     FileB,
                                                     MiddleSnakeX,
                                                     UpperA,
                                                     MiddleSnakeY,
                                                     UpperB,
                                                     DownVector,
                                                     UpVector);

        if (Status != 0) {
            TotalStatus = Status;
        }
    }

    return TotalStatus;
}

VOID
DiffComputeShortestMiddleSnake (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    INTN LowerA,
    INTN UpperA,
    INTN LowerB,
    INTN UpperB,
    PINTN DownVector,
    PINTN UpVector,
    PINTN MiddleSnakeX,
    PINTN MiddleSnakeY
    )

/*++

Routine Description:

    This routine implements the crux of the Myers' algorithm for computing the
    longest common subsequence in linear space, which is computing the shortest
    middle snake. Let's explore the algorithm a bit.

    Introduction

    Computing the difference between two files is equivalent to asking minimum
    set of steps it would take to transform one file into the other. Minimum
    being the tricky part (as in it's not good enough to say "delete all the
    lines from file A and replace them with all the lines from file B"). It
    turns out this problem is simple with the Longest Common Subsequence of the
    two sequences. For example, the longest common subsequence of BANANA and
    CATANA is AANA. Everything not in the longest common subsequence is the
    diff. The Myers' diff paper sets out to solve the problem of finding an
    LCS efficiently.

    Visualization:

    Arrange the two sequences with one along the X axis and one along the Y
    axis. Moving horizontally along the grid represents a single deletion, and
    moving vertically represents an addition. Diagonal moves can be made when
    the sequences are equal. Finding the longest common subsequence is then a
    matter of tracing a path from the top left to the bottom right using as few
    horizontal and vertical moves as possible (and therefore as many diagonals
    as possible). Below is an example trace through the sequences ABCABBA and
    CBABAC.

            A   B   C   A   B   B   A
        --------\   .   .   .   .   .
                  \
      C     .   .   \   .   .   .   .
                    |
      B     .   .   \   .   .   .   .
                      \
      A     .   .   .   \   .   .   .
                          \
      B     .   .   .   .   ----\   .
                                  \
      A     .   .   .   .   .   .   \
                                    |
      C     .   .   .   .   .   .   |

    Terms:

    Snake - A snake is a single horizontal or vertical move followed by zero
        or more diagonals. The algorithm is greedy, and does as many diagonal
        moves as possible.

    D Path - A D path is a path with D non-diagonal moves. So a 0 path moves
        only diagonally, a 1 path has one horizontal or vertical move and then
        a bunch of diagonals, etc. The maximum D path would be the maximum X
        plus the maximum Y (deleting all of the old stuff and replacing with
        all the new).

    k Line - This is the tricky one. K-lines run parallel to the 0 path (so
        they're diagonal). The value of k is the distance from the 0 path.
        So making one horizontal move and then travelling diagonally puts one
        on the k=1 line. Making a vertical move lands on the k=-1 line. Two
        horizontal moves is the k=2 line, etc. In fact, k = x - y. K lines
        really are just plots of y = x - k.

    V - V is a vector indexed by k (weird, right). At any given D, the vector V
        saves the coordinate of the farthest reaching snake for that k line.
        For early D values like 1, the only reachable k lines are 1 and -1. As
        D increases, the number of accessible k lines increases by two (going
        all the way right gets to a new k line as does going all the way down).
        The maximum reachable k lines ever are +/-D (going all the way right or
        all the way down), so this is the size of V.

    Quadratic space algorithm:

    A naive algorithm woulc simply explore every possible path from the top
    left to the bottom right (moving only right, down, and diagonally). This
    algorithm isn't too far off from there, it's just greedy and at every step
    it takes as many diagonals as it can. The quadratic space algorithm
    computes the vector V for successive D values until some index of V hits
    the lower right corner. It basically says, "try going both right and down,
    then take as many diagonals as possible. Recurse on the two new endpoints
    created." One interesting thing to note is that computing a new value in V
    can be done by taking its adjacent values (for one k below and one k above,
    ie one move left and one move up), and running snakes off those values. V
    is computed in steps of 2 (since it's not possible to get to a k=2 line
    from a D=1 path). So computing V only requires the previous value of V. If
    all that's desired is the length of the LCS, then just a single V array is
    needed. The only reason all the old V arrays are kept around after each
    step is to trace the path when finished.

    Linear space optimization:

    One observation that should be easy to understand is that the algorithm can
    also be run in reverse; start at the lower right corner and works towards
    the upper left. The resulting path may be different as different snakes are
    investigated. The linear space optimization involves running the algorithm
    forward and reverse at the same time until they overlap somewhere in the
    middle. Only the point at which they collide is needed, so only the last
    V is saved while running forward and reverse (this is what makes it linear).
    The magic is that this single overlapping snake somewhere in the middle is
    part of the optimal solution. So the algorithm found one piece of the
    solution in the middle. Then it's simply a matter of solving the two new
    smaller rectangles created in the upper left and lower right corners
    recursively until the solution is trivial.

Arguments:

    Context - Supplies a pointer to the diff application context.

    FileA - Supplies a pointer to the first file.

    FileB - Supplies a pointer to the second file.

    LowerA - Supplies the starting index within file A to work on.

    UpperA - Supplies the ending index within file A to work on, exclusive.

    LowerB - Supplies the starting index within file B to work on.

    UpperB - Supplies the ending index within file B to work on, exclusive.

    DownVector - Supplies the k-indexed vector for computing the shortest
        middle snake from the top down.

    UpVector - Supplies the k-indexed vector for computing the shortest middle
        snake from the bottom up.

    MiddleSnakeX - Supplies a pointer where the X coordinate (line index of
        file A) of the shortest middle snake will be returned.

    MiddleSnakeY - Supplies a pointer where the Y coordinate (line index of
        file B) of the shortest middle snake will be returned.

Return Value:

    None.

--*/

{

    INTN Delta;
    BOOL DeltaIsOdd;
    INTN DIndex;
    INTN DownK;
    INTN DownOffset;
    INTN KIndex;
    INTN Maximum;
    INTN MaximumD;
    INTN SnakeX;
    INTN SnakeY;
    INT Status;
    INTN UpK;
    INTN UpOffset;

    //
    // The maximum D value would be going all the way right and all the way
    // down (the files are entirely different).
    //

    Maximum = FileA->LineCount + FileB->LineCount + 1;

    //
    // Compute the K lines to start the forward (down) and reverse (up)
    // searches.
    //

    DownK = LowerA - LowerB;
    UpK = UpperA - UpperB;

    //
    // Delta is the difference in k between the start point and the end point.
    // This is needed to know which indices of the vector to check for overlap.
    //

    Delta = (UpperA - LowerA) - (UpperB - LowerB);
    DeltaIsOdd = FALSE;
    if ((Delta & 0x1) != 0) {
        DeltaIsOdd = TRUE;
    }

    //
    // In the paper, k values can go from -D to D. Use offsets to avoid
    // actually accessing negative array values.
    //

    DownOffset = Maximum - DownK;
    UpOffset = Maximum - UpK;

    //
    // Running the algorithm forward and reverse is guaranteed to cross
    // somewhere before D / 2.
    //

    MaximumD = (((UpperA - LowerA) + (UpperB - LowerB)) / 2) + 1;

    //
    // Initialize the vectors.
    //

    DownVector[DownOffset + DownK + 1] = LowerA;
    UpVector[UpOffset + UpK - 1] = UpperA;

    //
    // Iterate through successive D values until an overlap is found. This is
    // guaranteed to be the shortest path because it has the lowest D value.
    //

    for (DIndex = 0; DIndex <= MaximumD; DIndex += 1) {

        //
        // Run the algorithm forward. Compute all the coordinates for each
        // k line between -D and D in steps of two.
        //

        for (KIndex = DownK - DIndex; KIndex <= DownK + DIndex; KIndex += 2) {

            //
            // Use the better of the two x coordinates of the adjacent k lines,
            // being careful at the edges to avoid comparing against
            // impossible (unreachable) k lines.
            //

            if (KIndex == DownK - DIndex) {

                //
                // Take the same x coordinate as the k line above (meaning go
                // down).
                //

                SnakeX = DownVector[DownOffset + KIndex + 1];

            } else {

                //
                // Take the 1 + the x coordinate below, meaning go right.
                // Switch to going down if it is possible and better (starts
                // further). In a tie, go down.
                //

                SnakeX = DownVector[DownOffset + KIndex - 1] + 1;
                if ((KIndex < DownK + DIndex) &&
                    (DownVector[DownOffset + KIndex + 1] >= SnakeX)) {

                    SnakeX = DownVector[DownOffset + KIndex + 1];
                }
            }

            SnakeY = SnakeX - KIndex;

            //
            // Take as many diagonals as possible.
            //

            while ((SnakeX < UpperA) && (SnakeY < UpperB)) {
                Status = DiffCompareLines(Context,
                                          FileA,
                                          FileB,
                                          SnakeX,
                                          SnakeY);

                if (Status != 0) {
                    break;
                }

                SnakeX += 1;
                SnakeY += 1;
            }

            DownVector[DownOffset + KIndex] = SnakeX;

            //
            // Check for overlap.
            //

            if ((DeltaIsOdd != FALSE) &&
                (KIndex > UpK - DIndex) && (KIndex < UpK + DIndex)) {

                if (UpVector[UpOffset + KIndex] <=
                    DownVector[DownOffset + KIndex]) {

                    *MiddleSnakeX = DownVector[DownOffset + KIndex];
                    *MiddleSnakeY = *MiddleSnakeX - KIndex;
                    return;
                }
            }
        }

        //
        // Run the algorithm in reverse. Compute all the coordinates for each k
        // line between -D and D in steps of two.
        //

        for (KIndex = UpK - DIndex; KIndex <= UpK + DIndex; KIndex += 2) {

            //
            // Decide whether to take the path from the bottom or right.
            //

            if (KIndex == UpK + DIndex) {

                //
                // Take the x position from the lower k line, meaning go up.
                //

                SnakeX = UpVector[UpOffset + KIndex - 1];

            } else {

                //
                // Go right, unless going up is better.
                //

                SnakeX = UpVector[UpOffset + KIndex + 1] - 1;
                if ((KIndex > UpK - DIndex) &&
                    (UpVector[UpOffset + KIndex - 1] < SnakeX)) {

                    SnakeX = UpVector[UpOffset + KIndex - 1];
                }
            }

            SnakeY = SnakeX - KIndex;

            //
            // Take as many diagonals as possible.
            //

            while ((SnakeX > LowerA) && (SnakeY > LowerB)) {
                Status = DiffCompareLines(Context,
                                          FileA,
                                          FileB,
                                          SnakeX - 1,
                                          SnakeY - 1);

                if (Status != 0) {
                    break;
                }

                SnakeX -= 1;
                SnakeY -= 1;
            }

            UpVector[UpOffset + KIndex] = SnakeX;

            //
            // Check for overlap.
            //

            if ((DeltaIsOdd == FALSE) &&
                (KIndex >= DownK - DIndex) && (KIndex <= DownK + DIndex)) {

                if (UpVector[UpOffset + KIndex] <=
                    DownVector[DownOffset + KIndex]) {

                    *MiddleSnakeX = DownVector[DownOffset + KIndex];
                    *MiddleSnakeY = *MiddleSnakeX - KIndex;
                    return;
                }
            }
        }
    }

    //
    // A middle snake should always be found.
    //

    assert(FALSE);

    return;
}

INT
DiffCompareLines (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    UINTN LineIndexA,
    UINTN LineIndexB
    )

/*++

Routine Description:

    This routine compares two diff lines for equality.

Arguments:

    Context - Supplies a pointer to the diff application context.

    FileA - Supplies a pointer to the first file.

    FileB - Supplies a pointer to the second file.

    LineIndexA - Supplies the index of the line in the first file.

    LineIndexB - Supplies the index of the line in the second file.

Return Value:

    0 if the lines are equal.

    1 if the linse are not equal.

--*/

{

    UINTN IndexA;
    UINTN IndexB;
    PDIFF_LINE LineA;
    PDIFF_LINE LineB;

    assert(LineIndexA < FileA->LineCount);
    assert(LineIndexB < FileB->LineCount);

    LineA = FileA->Lines[LineIndexA];
    LineB = FileB->Lines[LineIndexB];

    //
    // If the hashes are not equal, then the lines are definitely not equal.
    // Easy peasy.
    //

    if (LineA->Hash != LineB->Hash) {
        return 1;
    }

    //
    // If this is the last line for either of them and there's not a newline at
    // the end, then it's probably different.
    //

    if ((LineIndexA == FileA->LineCount - 1) &&
        (FileA->NoNewlineAtEnd != FALSE)) {

        if ((LineIndexB != FileB->LineCount - 1) ||
            (FileB->NoNewlineAtEnd == FALSE)) {

            return 1;
        }
    }

    if ((LineIndexB == FileB->LineCount - 1) &&
        (FileB->NoNewlineAtEnd != FALSE)) {

        if ((LineIndexA != FileA->LineCount - 1) ||
            (FileA->NoNewlineAtEnd == FALSE)) {

            return 1;
        }
    }

    //
    // If not ignoring blanks, then use strcmp, as it's probably a bit more
    // optimized.
    //

    if ((Context->Options & DIFF_OPTION_IGNORE_BLANKS) == 0) {

        //
        // The sizes being different is also a dead giveaway.
        //

        if (LineA->Size != LineB->Size) {
            return 1;
        }

        if (strcmp(LineA->Data, LineB->Data) == 0) {
            return 0;
        }

        return 1;
    }

    //
    // Loop through comparing everything except whitespace.
    //

    IndexA = 0;
    IndexB = 0;
    while (TRUE) {
        while (isspace(LineA->Data[IndexA]) != 0) {
            IndexA += 1;
        }

        while (isspace(LineB->Data[IndexB]) != 0) {
            IndexB += 1;
        }

        if (LineA->Data[IndexA] != LineB->Data[IndexB]) {
            return 1;
        }

        //
        // If either one of them is at the end, it's time to stop the loop.
        //

        if ((IndexA == LineA->Size - 1) || (IndexB == LineB->Size - 1)) {

            //
            // They're not equal if they're not both at the end. But zoom past
            // any whitespace first.
            //

            while (isspace(LineA->Data[IndexA]) != 0) {
                IndexA += 1;
            }

            while (isspace(LineB->Data[IndexB]) != 0) {
                IndexB += 1;
            }

            if ((IndexA != LineA->Size - 1) || (IndexB != LineB->Size - 1)) {
                return 1;
            }

            break;
        }

        IndexA += 1;
        IndexB += 1;
    }

    return 0;
}

//
// Routines for displaying diffs.
//

VOID
DiffPrintEdDiffs (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB
    )

/*++

Routine Description:

    This routine prints the precomputed differences of two files using the ed
    output format.

Arguments:

    Context - Supplies a pointer to the diff application context.

    DirectoryA - Supplies a pointer to the directory prefix for file A.

    FileA - Supplies a pointer to the first file object to compare.

    DirectoryB - Supplies a pointer to the directory prefix for file B.

    FileB - Supplies a pointer to the second file object to compare.

Return Value:

    None.

--*/

{

    INTN LineA;
    INTN LineB;
    PSTR LineData;
    INTN LineIndex;
    INTN SizeA;
    INTN SizeB;

    assert(Context->ContextLines == 0);

    LineA = 0;
    LineB = 0;
    SizeA = 0;
    SizeB = 0;
    while ((LineA < FileA->LineCount) || (LineB < FileB->LineCount)) {
        DiffFindNextHunk(Context, FileA, FileB, &LineA, &LineB, &SizeA, &SizeB);
        if ((SizeA == 0) && (SizeB == 0)) {

            assert((LineA == FileA->LineCount) && (LineB == FileB->LineCount));

            break;
        }

        //
        // If both files are modified, then it's a change.
        //

        if ((LineA < FileA->LineCount) &&
            (FileA->Lines[LineA]->Modified != FALSE) &&
            (LineB < FileB->LineCount) &&
            (FileB->Lines[LineB]->Modified != FALSE)) {

            assert((SizeA != 0) && (SizeB != 0));

            if (SizeA == 1) {
                printf("%ld", LineA + 1);

            } else {
                printf("%ld,%ld", LineA + 1, LineA + SizeA);
            }

            if (SizeB == 1) {
                printf("c%ld\n", LineB + 1);

            } else {
                printf("c%ld,%ld\n", LineB + 1, LineB + SizeB);
            }

        //
        // If only file A is modified, then it's a deletion.
        //

        } else if ((LineA < FileA->LineCount) &&
                   (FileA->Lines[LineA]->Modified != FALSE)) {

            assert((SizeA != 0) && (SizeB == 0));

            if (SizeA == 1) {
                printf("%ldd%ld\n", LineA + 1, LineB);

            } else {
                printf("%ld,%ldd%ld\n", LineA + 1, LineA + SizeA, LineB);
            }

        //
        // It must be an insertion.
        //

        } else {

            assert((LineB < FileB->LineCount) &&
                   (FileB->Lines[LineB]->Modified != FALSE));

            assert((SizeB != 0) && (SizeA == 0));

            if (SizeB == 1) {
                printf("%lda%ld\n", LineA, LineB + 1);

            } else {
                printf("%lda%ld,%ld\n", LineA, LineB + 1, LineB + SizeB);
            }
        }

        //
        // Print the contents.
        //

        for (LineIndex = 0; LineIndex < SizeA; LineIndex += 1) {

            assert(FileA->Lines[LineA + LineIndex]->Modified != FALSE);

            LineData = FileA->Lines[LineA + LineIndex]->Data;
            if ((Context->Options & DIFF_OPTION_COLOR) != 0) {
                SwPrintInColor(ConsoleColorDefault,
                               DIFF_DELETION_COLOR,
                               "< %s\n",
                               LineData);

            } else {
                printf("< %s\n", LineData);
            }
        }

        if ((SizeA != 0) &&
            (LineA + SizeA == FileA->LineCount) &&
            (FileA->NoNewlineAtEnd != FALSE)) {

            printf("\\ No newline at end of file\n");
        }

        if ((SizeA != 0) && (SizeB != 0)) {
            printf("---\n");
        }

        for (LineIndex = 0; LineIndex < SizeB; LineIndex += 1) {

            assert(FileB->Lines[LineB + LineIndex]->Modified != FALSE);

            LineData = FileB->Lines[LineB + LineIndex]->Data;
            if ((Context->Options & DIFF_OPTION_COLOR) != 0) {
                SwPrintInColor(ConsoleColorDefault,
                               DIFF_INSERTION_COLOR,
                               "> %s\n",
                               LineData);

            } else {
                printf("> %s\n", LineData);
            }
        }

        if ((SizeB != 0) &&
            (LineB + SizeB == FileB->LineCount) &&
            (FileB->NoNewlineAtEnd != FALSE)) {

            printf("\\ No newline at end of file\n");
        }

        //
        // Advance beyond this hunk.
        //

        LineA += SizeA;
        LineB += SizeB;
    }

    return;
}

VOID
DiffPrintContextDiffs (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB
    )

/*++

Routine Description:

    This routine prints the precomputed differences of two files using the
    context output format.

Arguments:

    Context - Supplies a pointer to the diff application context.

    DirectoryA - Supplies a pointer to the directory prefix for file A.

    FileA - Supplies a pointer to the first file object to compare.

    DirectoryB - Supplies a pointer to the directory prefix for file B.

    FileB - Supplies a pointer to the second file object to compare.

Return Value:

    None.

--*/

{

    BOOL ChangesPresent;
    INTN IndexA;
    INTN IndexB;
    INTN LineA;
    INTN LineB;
    PSTR LineData;
    CHAR Marker;
    struct tm *ModificationTime;
    INTN SizeA;
    INTN SizeB;
    CHAR Timestamp[CONTEXT_DIFF_TIMESTAMP_SIZE];

    //
    // Print the heading.
    //

    ModificationTime = localtime(&(FileA->ModificationTime));
    strftime(Timestamp,
             sizeof(Timestamp),
             CONTEXT_DIFF_TIMESTAMP_FORMAT,
             ModificationTime);

    if (DirectoryA != NULL) {
        printf("*** %s/%s\t%s\n", DirectoryA, FileA->Name, Timestamp);

    } else {
        printf("*** %s\t%s\n", FileA->Name, Timestamp);
    }

    ModificationTime = localtime(&(FileB->ModificationTime));
    strftime(Timestamp,
             sizeof(Timestamp),
             CONTEXT_DIFF_TIMESTAMP_FORMAT,
             ModificationTime);

    if (DirectoryB != NULL) {
        printf("--- %s/%s\t%s\n", DirectoryB, FileB->Name, Timestamp);

    } else {
        printf("--- %s\t%s\n", FileB->Name, Timestamp);
    }

    LineA = 0;
    LineB = 0;
    SizeA = 0;
    SizeB = 0;
    while ((LineA < FileA->LineCount) || (LineB < FileB->LineCount)) {
        DiffFindNextHunk(Context, FileA, FileB, &LineA, &LineB, &SizeA, &SizeB);
        if ((SizeA == 0) && (SizeB == 0)) {

            assert((LineA == FileA->LineCount) && (LineB == FileB->LineCount));

            break;
        }

        printf("***************\n");

        //
        // Print the top half of the change, the deletions with context.
        //

        if (SizeA <= 1) {
            printf("*** %ld ***\n", LineA + ((LineA + SizeA) != 0));

        } else {
            printf("*** %ld,%ld ***\n", LineA + 1, LineA + SizeA);
        }

        ChangesPresent = FALSE;
        for (IndexA = LineA; IndexA < LineA + SizeA; IndexA += 1) {
            if (FileA->Lines[IndexA]->Modified != FALSE) {
                ChangesPresent = TRUE;
                break;
            }
        }

        if (ChangesPresent != FALSE) {
            IndexB = LineB;
            for (IndexA = LineA; IndexA < LineA + SizeA; IndexA += 1) {
                LineData = FileA->Lines[IndexA]->Data;

                //
                // If the first file is not modified, it's context.
                //

                if (FileA->Lines[IndexA]->Modified == FALSE) {
                    Marker = ' ';
                    if ((IndexB < FileB->LineCount) &&
                        (FileB->Lines[IndexB]->Modified == FALSE)) {

                        IndexB += 1;
                    }

                //
                // The line in file A is modified, so it's a deletion. Look at
                // file B to figure out if it's a change or pure deletion.
                //

                } else {
                    if ((IndexB >= FileB->LineCount) ||
                        (FileB->Lines[IndexB]->Modified == FALSE)) {

                        Marker = '-';
                        if (IndexB < FileB->LineCount) {
                            IndexB += 1;
                        }

                    } else {
                        Marker = '!';
                        IndexB += 1;
                    }
                }

                if (((Context->Options & DIFF_OPTION_COLOR) != 0) &&
                    (Marker != ' ')) {

                    SwPrintInColor(ConsoleColorDefault,
                                   DIFF_DELETION_COLOR,
                                   "%c %s\n",
                                   Marker,
                                   LineData);

                } else {
                    printf("%c %s\n", Marker, LineData);
                }
            }

            if ((SizeA != 0) &&
                (IndexA == FileA->LineCount) &&
                (FileA->NoNewlineAtEnd != FALSE) &&
                (Marker != ' ')) {

                printf("\\ No newline at end of file\n");
            }
        }

        //
        // Print the bottom half of the change, the additions with context.
        //

        if (SizeB <= 1) {
            printf("--- %ld ---\n", LineB + ((LineB + SizeB) != 0));

        } else {
            printf("--- %ld,%ld ---\n", LineB + 1, LineB + SizeB);
        }

        ChangesPresent = FALSE;
        for (IndexB = LineB; IndexB < LineB + SizeB; IndexB += 1) {
            if (FileB->Lines[IndexB]->Modified != FALSE) {
                ChangesPresent = TRUE;
                break;
            }
        }

        if (ChangesPresent != FALSE) {
            IndexA = LineA;
            for (IndexB = LineB; IndexB < LineB + SizeB; IndexB += 1) {
                LineData = FileB->Lines[IndexB]->Data;

                //
                // If the first file is not modified, it's context.
                //

                if (FileB->Lines[IndexB]->Modified == FALSE) {
                    Marker = ' ';
                    if ((IndexA < FileA->LineCount) &&
                        (FileA->Lines[IndexA]->Modified == FALSE)) {

                        IndexA += 1;
                    }

                //
                // The line in file B is modified, so it's an insertion. Look at
                // file A to figure out if it's a change or pure insertion.
                //

                } else {
                    if ((IndexA >= FileA->LineCount) ||
                        (FileA->Lines[IndexA]->Modified == FALSE)) {

                        Marker = '+';
                        if (IndexA < FileA->LineCount) {
                            IndexA += 1;
                        }

                    } else {
                        Marker = '!';
                        IndexA += 1;
                    }
                }

                if (((Context->Options & DIFF_OPTION_COLOR) != 0) &&
                    (Marker != ' ')) {

                    SwPrintInColor(ConsoleColorDefault,
                                   DIFF_INSERTION_COLOR,
                                   "%c %s\n",
                                   Marker,
                                   LineData);

                } else {
                    printf("%c %s\n", Marker, LineData);
                }
            }

            if ((SizeB != 0) &&
                (IndexB == FileB->LineCount) &&
                (FileB->NoNewlineAtEnd != FALSE) &&
                (Marker != ' ')) {

                printf("\\ No newline at end of file\n");
            }
        }

        //
        // Advance beyond this hunk.
        //

        LineA += SizeA;
        LineB += SizeB;
    }

    return;
}

VOID
DiffPrintUnifiedDiffs (
    PDIFF_CONTEXT Context,
    PSTR DirectoryA,
    PDIFF_FILE FileA,
    PSTR DirectoryB,
    PDIFF_FILE FileB
    )

/*++

Routine Description:

    This routine prints the precomputed differences of two files using the
    unified output format.

Arguments:

    Context - Supplies a pointer to the diff application context.

    DirectoryA - Supplies a pointer to the directory prefix for file A.

    FileA - Supplies a pointer to the first file object to compare.

    DirectoryB - Supplies a pointer to the directory prefix for file B.

    FileB - Supplies a pointer to the second file object to compare.

Return Value:

    None.

--*/

{

    INTN IndexA;
    INTN IndexB;
    INTN LineA;
    INTN LineB;
    PSTR LineData;
    struct tm *ModificationTime;
    INTN SizeA;
    INTN SizeB;
    CHAR Timestamp[CONTEXT_DIFF_TIMESTAMP_SIZE];

    //
    // Print the heading.
    //

    ModificationTime = localtime(&(FileA->ModificationTime));
    strftime(Timestamp,
             sizeof(Timestamp),
             CONTEXT_DIFF_TIMESTAMP_FORMAT,
             ModificationTime);

    if (DirectoryA != NULL) {
        printf("--- %s/%s\t%s\n", DirectoryA, FileA->Name, Timestamp);

    } else {
        printf("--- %s\t%s\n", FileA->Name, Timestamp);
    }

    ModificationTime = localtime(&(FileB->ModificationTime));
    strftime(Timestamp,
             sizeof(Timestamp),
             CONTEXT_DIFF_TIMESTAMP_FORMAT,
             ModificationTime);

    if (DirectoryB != NULL) {
        printf("+++ %s/%s\t%s\n", DirectoryB, FileB->Name, Timestamp);

    } else {
        printf("+++ %s\t%s\n", FileB->Name, Timestamp);
    }

    LineA = 0;
    LineB = 0;
    SizeA = 0;
    SizeB = 0;
    while ((LineA < FileA->LineCount) || (LineB < FileB->LineCount)) {
        DiffFindNextHunk(Context, FileA, FileB, &LineA, &LineB, &SizeA, &SizeB);
        if ((SizeA == 0) && (SizeB == 0)) {

            assert((LineA == FileA->LineCount) && (LineB == FileB->LineCount));

            break;
        }

        //
        // Print the hunk marker.
        //

        if (SizeA == 0) {
            printf("@@ -%ld,0 ", LineA + (LineA != 0));

        } else if (SizeA == 1) {
            printf("@@ -%ld ", LineA + 1);

        } else {
            printf("@@ -%ld,%ld ", LineA + 1, SizeA);
        }

        if (SizeB == 0) {
            printf("+%ld,0 @@\n", LineB + (LineB != 0));

        } else if (SizeB == 1) {
            printf("+%ld @@\n", LineB + 1);

        } else {
            printf("+%ld,%ld @@\n", LineB + 1, SizeB);
        }

        IndexA = LineA;
        IndexB = LineB;
        while ((IndexA < LineA + SizeA) || (IndexB < LineB + SizeB)) {

            //
            // Print any context lines.
            //

            while ((IndexA < LineA + SizeA) &&
                   (FileA->Lines[IndexA]->Modified == FALSE) &&
                   (IndexB < LineB + SizeB) &&
                   (FileB->Lines[IndexB]->Modified == FALSE)) {

                LineData = FileA->Lines[IndexA]->Data;
                IndexA += 1;
                IndexB += 1;
                printf(" %s\n", LineData);
            }

            //
            // Print all deletion lines together.
            //

            while ((IndexA < LineA + SizeA) &&
                   (FileA->Lines[IndexA]->Modified != FALSE)) {

                LineData = FileA->Lines[IndexA]->Data;
                IndexA += 1;
                if ((Context->Options & DIFF_OPTION_COLOR) != 0) {
                    SwPrintInColor(ConsoleColorDefault,
                                   DIFF_DELETION_COLOR,
                                   "-%s\n",
                                   LineData);

                } else {
                    printf("-%s\n", LineData);
                }
            }

            if ((SizeA != 0) &&
                (IndexA == FileA->LineCount) &&
                (FileA->NoNewlineAtEnd != FALSE)) {

                printf("\\ No newline at end of file\n");
            }

            //
            // Print all insertion lines together.
            //

            while ((IndexB < LineB + SizeB) &&
                   (FileB->Lines[IndexB]->Modified != FALSE)) {

                LineData = FileB->Lines[IndexB]->Data;
                IndexB += 1;
                if ((Context->Options & DIFF_OPTION_COLOR) != 0) {
                    SwPrintInColor(ConsoleColorDefault,
                                   DIFF_INSERTION_COLOR,
                                   "+%s\n",
                                   LineData);

                } else {
                    printf("+%s\n", LineData);
                }
            }

            if ((SizeB != 0) &&
                (IndexB == FileB->LineCount) &&
                (FileB->NoNewlineAtEnd != FALSE)) {

                printf("\\ No newline at end of file\n");
            }
        }

        //
        // Advance beyond this hunk.
        //

        LineA += SizeA;
        LineB += SizeB;
    }

    return;
}

VOID
DiffFindNextHunk (
    PDIFF_CONTEXT Context,
    PDIFF_FILE FileA,
    PDIFF_FILE FileB,
    PINTN LineA,
    PINTN LineB,
    PINTN SizeA,
    PINTN SizeB
    )

/*++

Routine Description:

    This routine finds the next diff hunk of two precomputed diffs.

Arguments:

    Context - Supplies a pointer to the diff application context.

    FileA - Supplies a pointer to the first file object.

    FileB - Supplies a pointer to the second file object.

    LineA - Supplies a pointer that on input contains the zero-based line of
        A to start from. On output, this will contain the line index of the
        next hunk.

    LineB - Supplies a pointer that on input contains the zero-based line of
        B to start from. On output, this will contain the line index of the
        next hunk.

    SizeA - Supplies a pointer where the size of the next hunk for A in lines
        will be returned.

    SizeB - Supplies a pointer where the size of the next hunk for B in lines
        will be returned.

Return Value:

    None.

--*/

{

    INTN ContextA;
    INTN ContextB;
    INTN ContextLines;
    INTN OriginalLineA;
    INTN OriginalLineB;

    *SizeA = 0;
    *SizeB = 0;
    OriginalLineA = *LineA;
    OriginalLineB = *LineB;
    while ((*LineA < FileA->LineCount) && (*LineB < FileB->LineCount)) {

        //
        // If either line is modified, then a hunk has been found.
        //

        if ((FileA->Lines[*LineA]->Modified != FALSE) ||
            (FileB->Lines[*LineB]->Modified != FALSE)) {

            break;
        }

        *LineA += 1;
        *LineB += 1;
    }

    //
    // Loop advancing past the diff and lines of context.
    //

    while (TRUE) {

        //
        // Advance through modified lines in A.
        //

        while ((*LineA + *SizeA < FileA->LineCount) &&
               (FileA->Lines[*LineA + *SizeA]->Modified != FALSE)) {

            *SizeA += 1;
        }

        //
        // Advance through modified lines in B.
        //

        while ((*LineB + *SizeB < FileB->LineCount) &&
               (FileB->Lines[*LineB + *SizeB]->Modified != FALSE)) {

            *SizeB += 1;
        }

        //
        // Now try to advance through the lines of context as well. If a
        // modification is found in either line, then the next hunk blurs into
        // this one. Look past two sets of context lines because any fewer and
        // the next hunk will bleed back up into this one.
        //

        ContextLines = 0;
        ContextA = 0;
        ContextB = 0;
        while (ContextLines < (Context->ContextLines * 2) + 1) {
            if ((*LineA + *SizeA + ContextA < FileA->LineCount) &&
                (FileA->Lines[*LineA + *SizeA + ContextA]->Modified != FALSE)) {

                break;
            }

            if ((*LineB + *SizeB + ContextB < FileB->LineCount) &&
                (FileB->Lines[*LineB + *SizeB + ContextB]->Modified != FALSE)) {

                break;
            }

            if (*LineA + *SizeA + ContextA < FileA->LineCount) {
                ContextA += 1;
            }

            if (*LineB + *SizeB + ContextB < FileB->LineCount) {
                ContextB += 1;
            }

            ContextLines += 1;
        }

        assert(*LineA + *SizeA + ContextA <= FileA->LineCount);
        assert(*LineB + *SizeB + ContextB <= FileB->LineCount);

        if ((ContextLines == (Context->ContextLines * 2) + 1) ||
            ((ContextA == 0) && (ContextB == 0))) {

            //
            // Add up to the requested amount of context lines to the size.
            //

            if (ContextA > Context->ContextLines) {
                ContextA = Context->ContextLines;
            }

            if (ContextB > Context->ContextLines) {
                ContextB = Context->ContextLines;
            }

            *SizeA += ContextA;
            *SizeB += ContextB;
            break;

        //
        // Otherwise, add all the context lines consumed to the hunk.
        //

        } else {
            *SizeA += ContextA;
            *SizeB += ContextB;
        }
    }

    //
    // If both hunks are of size zero, then there were no more diffs.
    //

    if ((*SizeA == 0) && (*SizeB == 0)) {
        return;
    }

    //
    // Also back up to provide the context lines at the beginning.
    //

    ContextLines = Context->ContextLines;
    if (ContextLines > *LineA) {
        ContextLines = *LineA;
    }

    if (*LineA - ContextLines < OriginalLineA) {
        ContextLines = *LineA - OriginalLineA;
    }

    *LineA -= ContextLines;
    *SizeA += ContextLines;
    ContextLines = Context->ContextLines;
    if (ContextLines > *LineB) {
        ContextLines = *LineB;
    }

    if (*LineB - ContextLines < OriginalLineB) {
        ContextLines = *LineB - OriginalLineB;
    }

    *LineB -= ContextLines;
    *SizeB += ContextLines;
    return;
}


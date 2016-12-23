/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ls.c

Abstract:

    This module implements the ls utility.

Author:

    Evan Green 25-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "ls.h"
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define LS_VERSION_MAJOR 1
#define LS_VERSION_MINOR 0

#define LS_USAGE \
    "usage: ls [-CFRacdilqrtu1][-H | -L][-fgmnopsx] [file...]\n\n"             \
    "Options:\n"                                                               \
    "  -a, --all -- Include names that begin with a period.\n"                 \
    "  -C -- Display as multi-text-column output, sorted down the columns.\n"  \
    "  --color=[when] -- Display items in color. Arguments can be always, \n"  \
    "        auto, or never.\n"                                                \
    "  -c -- Show file status change time instead of modification time.\n"     \
    "  -d, --directory -- Treat directories specified as operands the same \n" \
    "        as files are treated. Don't follow symbolic links unless -H or \n"\
    "        -L is specified.\n"                                               \
    "  -F, --classify -- Write a '/' after directories, a '*' after \n"        \
    "        executables, a '|' fter FIFOs, and a '@' after symbolic links.\n" \
    "  -f -- Disable sorting. Turns off -l, -t, -s, and -r, and turns on -a.\n"\
    "  -g -- Same as -l but don't print the owner.\n"                          \
    "  -H, --dereference-command-line -- Follow symbolic links found in \n"    \
    "        command line arguments.\n"                                        \
    "  -i, --inode -- Print file serial numbers.\n"                            \
    "  -L, --dereference -- Always follow symbolic links.\n"                   \
    "  -l -- Show the output in long format. Turns on -1, and does not\n"      \
    "        follow symlinks unless -H or -L is specified.\n"                  \
    "  -m -- List results separated by commas.\n"                              \
    "  -n, --numeric-uid-gid -- Write out the owner and group UID and GID, \n" \
    "        instead of their associated character names.\n"                   \
    "  -o -- Same as -l, but don't print the group.\n"                         \
    "  -p -- Write a slash '/' after all directories.\n"                       \
    "  -q, --hide-control-characters -- Print non-printable characters and \n" \
    "        tabs as '?'.\n"                                                   \
    "  -R, --recursive -- Recursively list subdirectories.\n"                  \
    "  -r, --reverse -- Reverse the sort order.\n"                             \
    "  -s, --size -- Print the file block count for each file.\n"              \
    "  -t -- Sort with the primary key as the modification (or creation, or \n"\
    "        access) time, with a secondary key of the file name.\n"           \
    "  -u -- Use the last access time instead of modification time.\n"         \
    "  -x -- Sort entries across rather than down for column-based output.\n"  \
    "  -1 -- Display one entry per line.\n"                                    \
    "  --help -- Display this help text.\n"                                    \
    "  --version -- Display the version number and exit.\n"

#define LS_OPTIONS_STRING "CFHLRacdfgilmnopqrstux1"

#define LS_INITIAL_FILES_COUNT 16
#define LS_INITIAL_TRAVERSED_DIRECTORIES_COUNT 16

#define LS_DATE_STRING_SIZE 13

#define LS_DEFAULT_MAX_WIDTH 80
#define LS_COLUMN_PADDING 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
int
(*PLS_SORT_COMPARE_FUNCTION) (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two ls files.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
LsCategorize (
    PLS_CONTEXT Context,
    PSTR Argument
    );

INT
LsList (
    PLS_CONTEXT Context
    );

INT
LsListDirectory (
    PLS_CONTEXT Context,
    PSTR DirectoryPath
    );

INT
LsListFiles (
    PLS_CONTEXT Context,
    PLS_FILE *Files,
    ULONG FileCount,
    BOOL PrintTotal
    );

VOID
LsListFile (
    PLS_CONTEXT Context,
    PLS_FILE File
    );

VOID
LsPrintPermissions (
    PLS_FILE File
    );

VOID
LsPrintDate (
    PLS_CONTEXT Context,
    time_t Date
    );

VOID
LsPrintFileName (
    PLS_CONTEXT Context,
    PLS_FILE File
    );

PLS_CONTEXT
LsCreateContext (
    );

VOID
LsDestroyContext (
    PLS_CONTEXT Context
    );

PLS_FILE
LsCreateFileInformation (
    PLS_CONTEXT Context,
    PSTR FileName,
    ULONG FileNameSize,
    PSTR LinkDestination,
    ULONG LinkDestinationSize,
    BOOL LinkBroken,
    struct stat *Stat
    );

VOID
LsDestroyFileInformation (
    PLS_FILE File
    );

VOID
LsAddTraversedDirectory (
    PLS_CONTEXT Context,
    ino_t Directory
    );

BOOL
LsHasDirectoryBeenTraversed (
    PLS_CONTEXT Context,
    ino_t Directory
    );

ULONG
LsGetCharacterCountForInteger (
    ULONGLONG Integer
    );

//
// -------------------------------------------------------------------- Globals
//

struct option LsLongOptions[] = {
    {"color", optional_argument, 0, '2'},
    {"classify", no_argument, 0, 'F'},
    {"dereference-command-line", no_argument, 0, 'H'},
    {"dereference", no_argument, 0, 'L'},
    {"recursive", no_argument, 0, 'R'},
    {"all", no_argument, 0, 'a'},
    {"directory", no_argument, 0, 'd'},
    {"inode", no_argument, 0, 'i'},
    {"numeric-uid-gid", no_argument, 0, 'n'},
    {"hide-control-characters", no_argument, 0, 'q'},
    {"reverse", no_argument, 0, 'r'},
    {"size", no_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
LsMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the ls (list directory)
    utility.

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
    ULONG ArgumentIndex;
    PLS_CONTEXT Context;
    ULONG ListCount;
    INT Option;
    INT ReturnValue;

    ReturnValue = ENOMEM;
    Context = LsCreateContext();
    if (Context == NULL) {
        goto MainEnd;
    }

    ListCount = 0;

    //
    // Loop through all the options.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             LS_OPTIONS_STRING,
                             LsLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            ReturnValue = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'C':
            Context->Flags |= LS_OPTION_COLUMN_OUTPUT;
            Context->Flags &= ~(LS_OPTION_ONE_ENTRY_PER_LINE |
                                LS_OPTION_LONG_FORMAT |
                                LS_OPTION_COMMA_SEPARATED);

            break;

        case 'F':
            Context->Flags |= LS_OPTION_DECORATE_NAMES |
                              LS_OPTION_DECORATE_DIRECTORIES;

            break;

        case 'H':
            Context->Flags |= LS_OPTION_FOLLOW_LINKS_IN_OPERANDS;
            break;

        case 'L':
            Context->Flags |= LS_OPTION_FOLLOW_LINKS_IN_OPERANDS |
                              LS_OPTION_FOLLOW_LINKS_IN_LIST;

            break;

        case 'R':
            Context->Flags |= LS_OPTION_RECURSIVE |
                              LS_OPTION_PRINT_DIRECTORY_NAME;

            break;

        case 'a':
            Context->Flags |= LS_OPTION_LIST_ALL;
            break;

        case 'c':
            Context->Flags |= LS_OPTION_USE_STATUS_CHANGE_TIME;
            Context->Flags &= ~LS_OPTION_USE_ACCESS_TIME;
            break;

        case 'd':
            Context->Flags &= ~(LS_OPTION_FOLLOW_LINKS_IN_OPERANDS |
                                LS_OPTION_FOLLOW_LINKS_IN_LIST);

            Context->Flags |= LS_OPTION_ALL_OPERANDS_AS_FILE;
            break;

        case 'f':
            Context->Flags |= LS_OPTION_LIST_ALL |
                              LS_OPTION_COLUMN_OUTPUT |
                              LS_OPTION_NO_SORTING;

            Context->Flags &= ~(LS_OPTION_LONG_FORMAT |
                                LS_OPTION_SORT_BY_DATE |
                                LS_OPTION_PRINT_BLOCK_COUNT |
                                LS_OPTION_REVERSE_SORT |
                                LS_OPTION_ONE_ENTRY_PER_LINE);

            break;

        case 'i':
            Context->Flags |= LS_OPTION_INCLUDE_SERIAL_NUMBERS;
            break;

        case 'o':
        case 'g':
        case 'l':
            Context->Flags |= LS_OPTION_ONE_ENTRY_PER_LINE |
                              LS_OPTION_LONG_FORMAT;

            if (Option == 'o') {
                Context->Flags |= LS_OPTION_SKIP_GROUP;

            } else if (Option == 'g') {
                Context->Flags |= LS_OPTION_SKIP_OWNER;
            }

            Context->Flags &= ~LS_OPTION_COLUMN_OUTPUT;
            break;

        case 'm':
            Context->Flags |= LS_OPTION_COMMA_SEPARATED;
            Context->Flags &= ~LS_OPTION_COLUMN_OUTPUT;
            break;

        case 'n':
            Context->Flags |= LS_OPTION_PRINT_USER_GROUP_NUMBERS;
            break;

        case 'p':
            Context->Flags |= LS_OPTION_DECORATE_DIRECTORIES;
            break;

        case 'q':
            Context->Flags |= LS_OPTION_PRINT_QUESTION_MARKS;
            break;

        case 'r':
            Context->Flags |= LS_OPTION_REVERSE_SORT;
            break;

        case 's':
            Context->Flags |= LS_OPTION_PRINT_BLOCK_COUNT;
            break;

        case 't':
            Context->Flags |= LS_OPTION_SORT_BY_DATE;
            break;

        case 'u':
            Context->Flags |= LS_OPTION_USE_ACCESS_TIME;
            Context->Flags &= ~LS_OPTION_USE_STATUS_CHANGE_TIME;
            break;

        case 'x':
            Context->Flags |= LS_OPTION_SORT_COLUMNS_ACROSS;
            break;

        case '1':
            Context->Flags |= LS_OPTION_ONE_ENTRY_PER_LINE;
            break;

        case '2':
            Argument = optarg;
            if (Argument != NULL) {
                if (strcasecmp(Argument, "always") == 0) {
                    Context->Flags |= LS_OPTION_COLOR;

                } else if (strcasecmp(Argument, "never") == 0) {
                    Context->Flags &= ~LS_OPTION_COLOR;
                }
            }

            break;

        case 'V':
            SwPrintVersion(LS_VERSION_MAJOR, LS_VERSION_MINOR);
            return 1;

        case 'h':
            printf(LS_USAGE);
            return 1;

        default:

            assert(FALSE);

            ReturnValue = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    ListCount = ArgumentCount - ArgumentIndex;

    //
    // Print directory names if there's more than one argument to list.
    //

    if (ListCount > 1) {
        Context->Flags |= LS_OPTION_PRINT_DIRECTORY_NAME;
    }

    //
    // Now that the options have been figured out, loop through again to
    // actually print the files and directories requested.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];

        //
        // Categorize this as either a file or a directory. Errors are not
        // treated fatally.
        //

        LsCategorize(Context, Argument);
        ArgumentIndex += 1;
    }

    //
    // If nothing was printed, then categorize and print the current directory.
    //

    if (ListCount == 0) {
        LsCategorize(Context, ".");
    }

    //
    // Finally, list everything that's been built up.
    //

    ReturnValue = LsList(Context);

MainEnd:
    if (Context != NULL) {
        LsDestroyContext(Context);
    }

    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
LsCategorize (
    PLS_CONTEXT Context,
    PSTR Argument
    )

/*++

Routine Description:

    This routine categorizes the argument as either a file or a directory.

Arguments:

    Context - Supplies a pointer to the current context.

    Argument - Supplies a pointer to the string containing the item to
        categorize.

Return Value:

    0 on success.

    Non-zero if there was an error.

--*/

{

    PLS_FILE File;
    BOOL FollowLinks;
    BOOL IsDirectory;
    BOOL LinkBroken;
    PSTR LinkDestination;
    ULONG LinkDestinationSize;
    struct stat LinkStat;
    PVOID NewBuffer;
    INT Result;
    struct stat Stat;

    LinkBroken = FALSE;
    LinkDestination = NULL;
    LinkDestinationSize = 0;
    FollowLinks = FALSE;
    if ((Context->Flags & LS_OPTION_FOLLOW_LINKS_IN_OPERANDS) != 0) {
        FollowLinks = TRUE;
    }

    //
    // Figure out if this argument is a file or a directory.
    //

    Result = SwStat(Argument, FollowLinks, &Stat);
    if (Result != 0) {
        Result = errno;
        SwPrintError(Result, Argument, "Cannot stat");
        return Result;
    }

    IsDirectory = FALSE;
    if ((Context->Flags & LS_OPTION_ALL_OPERANDS_AS_FILE) == 0) {

        //
        // Figure out if this is a directory or a file. Follow a link if
        // requested.
        //

        if (S_ISLNK(Stat.st_mode)) {
            if (SwStat(Argument, TRUE, &LinkStat) != 0) {
                LinkBroken = TRUE;
            }

            Result = SwReadLink(Argument, &LinkDestination);
            if (Result != 0) {
                Result = errno;
                SwPrintError(Result, Argument, "Cannot read link");
                goto CategorizeEnd;
            }

            LinkDestinationSize = strlen(LinkDestination) + 1;
        }

        if (S_ISDIR(Stat.st_mode)) {
            IsDirectory = TRUE;
        }
    }

    //
    // Add it to the right array.
    //

    if (IsDirectory != FALSE) {
        if (Context->DirectoriesSize >= Context->DirectoriesCapacity) {
            if (Context->DirectoriesCapacity == 0) {
                Context->DirectoriesCapacity = LS_INITIAL_FILES_COUNT;

            } else {
                Context->DirectoriesCapacity *= 2;
            }

            NewBuffer = realloc(Context->Directories,
                                Context->DirectoriesCapacity * sizeof(PSTR));

            if (NewBuffer == NULL) {
                Context->DirectoriesSize = 0;
                Context->DirectoriesCapacity = 0;
                Result = ENOMEM;
                goto CategorizeEnd;
            }

            Context->Directories = NewBuffer;
        }

        Context->Directories[Context->DirectoriesSize] = Argument;
        Context->DirectoriesSize += 1;

    } else {
        File = LsCreateFileInformation(Context,
                                       Argument,
                                       strlen(Argument) + 1,
                                       LinkDestination,
                                       LinkDestinationSize,
                                       LinkBroken,
                                       &Stat);

        if (File == NULL) {
            Result = ENOMEM;
            goto CategorizeEnd;
        }

        LinkDestination = NULL;
        if (Context->FilesSize >= Context->FilesCapacity) {
            if (Context->FilesCapacity == 0) {
                Context->FilesCapacity = LS_INITIAL_FILES_COUNT;

            } else {
                Context->FilesCapacity *= 2;
            }

            NewBuffer = realloc(Context->Files,
                                Context->FilesCapacity * sizeof(PLS_FILE));

            if (NewBuffer == NULL) {
                Context->FilesSize = 0;
                Context->FilesCapacity = 0;
                Result = ENOMEM;
                goto CategorizeEnd;
            }

            Context->Files = NewBuffer;
        }

        Context->Files[Context->FilesSize] = File;
        Context->FilesSize += 1;
    }

CategorizeEnd:
    if (LinkDestination != NULL) {
        free(LinkDestination);
    }

    return Result;
}

INT
LsList (
    PLS_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints the listing for the files and directories in the
    given context.

Arguments:

    Context - Supplies a pointer to the current context.

Return Value:

    0 on success.

    Non-zero if there was an error.

--*/

{

    ULONG DirectoryIndex;
    INT OverallResult;
    INT Result;

    OverallResult = 0;
    if (Context->FilesSize != 0) {
        OverallResult = LsListFiles(Context,
                                    Context->Files,
                                    Context->FilesSize,
                                    FALSE);
    }

    for (DirectoryIndex = 0;
         DirectoryIndex < Context->DirectoriesSize;
         DirectoryIndex += 1) {

        if ((Context->FilesSize != 0) || (DirectoryIndex != 0)) {
            printf("\n");
        }

        Result = LsListDirectory(Context, Context->Directories[DirectoryIndex]);
        if ((Result != 0) && (OverallResult == 0)) {
            OverallResult = Result;
        }
    }

    return OverallResult;
}

INT
LsListDirectory (
    PLS_CONTEXT Context,
    PSTR DirectoryPath
    )

/*++

Routine Description:

    This routine prints the contents for the given directory.

Arguments:

    Context - Supplies a pointer to the current context.

    DirectoryPath - Supplies a pointer to the path of the directory.

Return Value:

    0 on success.

    Non-zero if there was an error.

--*/

{

    DIR *Directory;
    ULONG DirectoryPathLength;
    struct dirent *Entry;
    ULONG EntryNameLength;
    PLS_FILE File;
    PLS_FILE *FileArray;
    ULONG FileArrayCapacity;
    ULONG FileArraySize;
    ULONG FileIndex;
    BOOL FollowLinks;
    PSTR FullPath;
    ULONG FullPathSize;
    BOOL LinkBroken;
    PSTR LinkDestination;
    ULONG LinkDestinationSize;
    struct stat LinkStat;
    PLS_FILE *NewFileArray;
    BOOL PrintTotal;
    INT Result;
    struct stat Stat;
    struct stat *StatPointer;

    FileArray = NULL;
    FileArrayCapacity = 0;
    FileArraySize = 0;
    FullPath = NULL;
    LinkDestination = NULL;
    LinkDestinationSize = 0;
    PrintTotal = FALSE;
    if (((Context->Flags & LS_OPTION_LONG_FORMAT) != 0) ||
        ((Context->Flags & LS_OPTION_PRINT_BLOCK_COUNT) != 0)) {

        PrintTotal = TRUE;
    }

    FollowLinks = FALSE;
    if ((Context->Flags & LS_OPTION_FOLLOW_LINKS_IN_LIST) != 0) {
        FollowLinks = TRUE;
    }

    //
    // Add this directory as having been traversed.
    //

    Result = SwStat(DirectoryPath, FALSE, &Stat);
    if (Result == 0) {
        LsAddTraversedDirectory(Context, Stat.st_ino);
    }

    //
    // Open up the directory.
    //

    Directory = opendir(DirectoryPath);
    if (Directory == NULL) {
        Result = errno;
        SwPrintError(Result, DirectoryPath, "Unable to open directory");
        goto ListDirectoryEnd;
    }

    DirectoryPathLength = strlen(DirectoryPath);
    if ((Context->Flags & LS_OPTION_PRINT_DIRECTORY_NAME) != 0) {
        printf("%s:\n", DirectoryPath);
    }

    //
    // Loop through the entries in the directory.
    //

    while (TRUE) {
        errno = 0;
        Entry = readdir(Directory);
        if (Entry == NULL) {
            Result = errno;
            if (Result != 0) {
                SwPrintError(Result, DirectoryPath, "Unable to read directory");
                goto ListDirectoryEnd;
            }

            break;
        }

        //
        // If the entry begins with a dot, skip it unless otherwise specified.
        //

        if ((Entry->d_name[0] == '.') &&
            ((Context->Flags & LS_OPTION_LIST_ALL) == 0)) {

            continue;
        }

        //
        // Create the full path to the file so it can be statted.
        //

        EntryNameLength = strlen(Entry->d_name);
        Result = SwAppendPath(DirectoryPath,
                              DirectoryPathLength + 1,
                              Entry->d_name,
                              EntryNameLength + 1,
                              &FullPath,
                              &FullPathSize);

        if (Result == FALSE) {
            Result = ENOMEM;
            goto ListDirectoryEnd;
        }

        LinkBroken = FALSE;
        Result = SwStat(FullPath, FollowLinks, &Stat);
        if (Result == 0) {
            StatPointer = &Stat;

            assert((FollowLinks != FALSE) || (Stat.st_ino == Entry->d_ino));

            //
            // Follow the link for the stat information.
            //

            if (S_ISLNK(Stat.st_mode)) {
                if (SwStat(FullPath, TRUE, &LinkStat) != 0) {
                    LinkBroken = TRUE;
                }

                Result = SwReadLink(FullPath, &LinkDestination);
                if (Result != 0) {
                    SwPrintError(Result, FullPath, "Failed to read link");
                }

                LinkDestinationSize = strlen(LinkDestination) + 1;
            }

        } else {
            StatPointer = NULL;
            SwPrintError(Result, FullPath, "Unable to stat");
        }

        free(FullPath);
        FullPath = NULL;

        //
        // Ensure there's enough room in the array for this upcoming entry.
        //

        if (FileArraySize == FileArrayCapacity) {
            if (FileArrayCapacity == 0) {
                FileArrayCapacity = LS_INITIAL_FILES_COUNT;

            } else {
                FileArrayCapacity *= 2;
            }

            NewFileArray = realloc(FileArray,
                                   FileArrayCapacity * sizeof(PLS_FILE));

            if (NewFileArray == NULL) {
                Result = ENOMEM;
                goto ListDirectoryEnd;
            }

            FileArray = NewFileArray;
        }

        //
        // Create the file information structure.
        //

        FileArray[FileArraySize] = LsCreateFileInformation(Context,
                                                           Entry->d_name,
                                                           EntryNameLength + 1,
                                                           LinkDestination,
                                                           LinkDestinationSize,
                                                           LinkBroken,
                                                           StatPointer);

        if (FileArray[FileArraySize] == NULL) {
            goto ListDirectoryEnd;
        }

        FileArraySize += 1;
        LinkDestination = NULL;
        LinkDestinationSize = 0;
    }

    //
    // List the files in here.
    //

    Result = LsListFiles(Context, FileArray, FileArraySize, PrintTotal);
    if (Result != 0) {
        goto ListDirectoryEnd;
    }

    //
    // Potentially recurse down subdirectories if requested
    //

    if ((Context->Flags & LS_OPTION_RECURSIVE) != 0) {
        for (FileIndex = 0; FileIndex < FileArraySize; FileIndex += 1) {
            File = FileArray[FileIndex];

            //
            // Skip the dot and dot-dot entries.
            //

            if (File->Name[0] == '.') {
                if (File->Name[1] == '\0') {
                    continue;

                } else if ((File->Name[1] == '.') && (File->Name[2] == '\0')) {
                    continue;
                }
            }

            if ((S_ISDIR(File->Stat.st_mode)) &&
                (LsHasDirectoryBeenTraversed(Context, File->Stat.st_ino) ==
                 FALSE)) {

                Result = SwAppendPath(DirectoryPath,
                                      DirectoryPathLength + 1,
                                      File->Name,
                                      File->NameSize,
                                      &FullPath,
                                      &FullPathSize);

                if (Result == FALSE) {
                    Result = ENOMEM;
                    goto ListDirectoryEnd;
                }

                printf("\n");
                Result = LsListDirectory(Context, FullPath);
                free(FullPath);
                FullPath = NULL;
                if (Result != 0) {
                    goto ListDirectoryEnd;
                }
            }
        }
    }

ListDirectoryEnd:
    if (LinkDestination != NULL) {
        free(LinkDestination);
    }

    if (FullPath != NULL) {
        free(FullPath);
    }

    if (Directory != NULL) {
        closedir(Directory);
    }

    if (FileArray != NULL) {
        for (FileIndex = 0; FileIndex < FileArraySize; FileIndex += 1) {
            LsDestroyFileInformation(FileArray[FileIndex]);
        }

        free(FileArray);
    }

    return Result;
}

INT
LsListFiles (
    PLS_CONTEXT Context,
    PLS_FILE *Files,
    ULONG FileCount,
    BOOL PrintTotal
    )

/*++

Routine Description:

    This routine lists a group of files as a directory.

Arguments:

    Context - Supplies a pointer to the application context.

    Files - Supplies an array of pointers to files to print.

    FileCount - Supplies the number of files in the array.

    PrintTotal - Supplies a boolean indicating whether the total number of
        512 byte blocks used by these files should be displayed.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONGLONG BlockCount;
    ULONG ColumnCount;
    ULONG ColumnWidth;
    PLS_SORT_COMPARE_FUNCTION CompareFunction;
    PLS_FILE File;
    ULONG FileIndex;
    ULONG MaxBlocksLength;
    ULONG MaxFileLength;
    ULONG MaxFileNumberLength;
    ULONG MaxFileSizeLength;
    ULONG MaxGroupLength;
    ULONG MaxHardLinkLength;
    ULONG MaxOwnerLength;
    int MaxWidth;
    ULONG NumberWidth;
    INT Result;
    BOOL Reverse;
    ULONG RoundedCount;
    PLS_FILE *RoundedFiles;
    ULONG Row;
    ULONG RowCount;
    ULONGLONG TotalBlockCount;
    ULONG TotalWidth;

    Context->NameColumnSize = 0;
    Context->FileNumberColumnSize = 0;
    Context->FileBlocksColumnSize = 0;
    Context->FileSizeColumnSize = 0;
    Context->HardLinkColumnSize = 0;
    Context->OwnerColumnSize = 0;
    Context->GroupColumnSize = 0;
    RoundedFiles = NULL;
    TotalBlockCount = 0;

    //
    // Figure out how wide the terminal is.
    //

    Result = SwGetTerminalDimensions(&MaxWidth, NULL);
    if (Result != 0) {
        MaxWidth = LS_DEFAULT_MAX_WIDTH;
    }

    //
    // Calculate some column widths.
    //

    MaxBlocksLength = 0;
    MaxFileLength = 0;
    MaxFileNumberLength = 0;
    MaxFileSizeLength = 0;
    MaxGroupLength = 0;
    MaxHardLinkLength = 0;
    MaxOwnerLength = 0;
    TotalWidth = 0;
    for (FileIndex = 0; FileIndex < FileCount; FileIndex += 1) {
        File = Files[FileIndex];

        assert(File->NameSize - 1 == strlen(File->Name));

        if (File->NameSize > MaxFileLength) {
            MaxFileLength = File->NameSize;
        }

        if ((Context->Flags & LS_OPTION_INCLUDE_SERIAL_NUMBERS)) {
            NumberWidth = LsGetCharacterCountForInteger(File->Stat.st_ino);
            if (NumberWidth + 1 > MaxFileNumberLength) {
                MaxFileNumberLength = NumberWidth + 1;
            }
        }

        BlockCount = SwGetBlockCount(&(File->Stat));
        TotalBlockCount += (BlockCount * SwGetBlockSize(&(File->Stat))) / 512;
        if ((Context->Flags & LS_OPTION_PRINT_BLOCK_COUNT) != 0) {
            NumberWidth = LsGetCharacterCountForInteger(BlockCount);
            if (NumberWidth + 1 > MaxBlocksLength) {
                MaxBlocksLength = NumberWidth + 1;
            }
        }

        if ((Context->Flags & LS_OPTION_LONG_FORMAT) != 0) {
            NumberWidth = LsGetCharacterCountForInteger(File->Stat.st_size);
            if (NumberWidth + 1 > MaxFileSizeLength) {
                MaxFileSizeLength = NumberWidth + 1;
            }

            NumberWidth = LsGetCharacterCountForInteger(File->Stat.st_nlink);
            if (NumberWidth + 1 > MaxHardLinkLength) {
                MaxHardLinkLength = NumberWidth + 1;
            }

            if (File->OwnerName != NULL) {
                if (File->OwnerNameSize > MaxOwnerLength) {
                    MaxOwnerLength = File->OwnerNameSize;
                }

            } else {
                NumberWidth = LsGetCharacterCountForInteger(File->Stat.st_uid);
                if (NumberWidth + 1 > MaxOwnerLength) {
                    MaxOwnerLength = NumberWidth + 1;
                }
            }

            if (File->GroupName != NULL) {
                if (File->GroupNameSize > MaxGroupLength) {
                    MaxGroupLength = File->GroupNameSize;
                }

            } else {
                NumberWidth = LsGetCharacterCountForInteger(File->Stat.st_gid);
                if (NumberWidth + 1 > MaxGroupLength) {
                    MaxGroupLength = NumberWidth + 1;
                }
            }
        }

        TotalWidth += Files[FileIndex]->NameSize;
    }

    //
    // Remove the null terminator from the name length calculation.
    //

    if (MaxFileLength != 0) {
        MaxFileLength -= 1;
    }

    //
    // If decorating everything or just directories, add a character of
    // space for that.
    //

    if ((Context->Flags &
         (LS_OPTION_DECORATE_DIRECTORIES | LS_OPTION_DECORATE_NAMES)) != 0) {

        MaxFileLength += 1;
    }

    Context->FileNumberColumnSize = MaxFileNumberLength;
    Context->FileBlocksColumnSize = MaxBlocksLength;
    Context->FileSizeColumnSize = MaxFileSizeLength;
    Context->HardLinkColumnSize = MaxHardLinkLength;
    Context->OwnerColumnSize = MaxOwnerLength;
    Context->GroupColumnSize = MaxGroupLength;
    ColumnWidth = MaxFileLength + MaxFileNumberLength + MaxBlocksLength;
    if ((Context->Flags & LS_OPTION_COLUMN_OUTPUT) != 0) {
        if (MaxFileLength > MaxWidth) {
            Context->NameColumnSize = 0;
            Context->ColumnCount = 1;

        } else if ((TotalWidth + (FileCount * LS_COLUMN_PADDING)) < MaxWidth) {
            Context->NameColumnSize = 0;
            Context->ColumnCount = FileCount;

        } else {
            Context->NameColumnSize = MaxFileLength;
            Context->ColumnCount = (MaxWidth - 1) /
                                   (ColumnWidth + LS_COLUMN_PADDING);
        }
    }

    //
    // Sort the output if desired.
    //

    RoundedCount = FileCount;
    RoundedFiles = Files;
    if (((Context->Flags & LS_OPTION_NO_SORTING) == 0) && (FileCount > 1)) {
        Reverse = FALSE;
        if ((Context->Flags & LS_OPTION_REVERSE_SORT) != 0) {
            Reverse = TRUE;
        }

        if ((Context->Flags & LS_OPTION_SORT_BY_DATE) != 0) {
            if ((Context->Flags & LS_OPTION_USE_ACCESS_TIME) != 0) {
                if (Reverse != FALSE) {
                    CompareFunction = LsCompareFilesByReverseAccessDate;

                } else {
                    CompareFunction = LsCompareFilesByAccessDate;
                }

            } else if ((Context->Flags &
                        LS_OPTION_USE_STATUS_CHANGE_TIME) != 0) {

                if (Reverse != FALSE) {
                    CompareFunction = LsCompareFilesByReverseStatusChangeDate;

                } else {
                    CompareFunction = LsCompareFilesByStatusChangeDate;
                }

            } else {
                if (Reverse != FALSE) {
                    CompareFunction = LsCompareFilesByReverseModificationDate;

                } else {
                    CompareFunction = LsCompareFilesByModificationDate;
                }
            }

        } else {
            if (Reverse != FALSE) {
                CompareFunction = LsCompareFilesByReverseName;

            } else {
                CompareFunction = LsCompareFilesByName;
            }
        }

        qsort(Files, FileCount, sizeof(PLS_FILE *), CompareFunction);

        //
        // Unless the user wanted the entries to go across, rotate the array
        // to make the list scan down each column.
        //

        if (((Context->Flags & LS_OPTION_COLUMN_OUTPUT) != 0) &&
            ((Context->Flags & LS_OPTION_SORT_COLUMNS_ACROSS) == 0)) {

            ColumnCount = Context->ColumnCount;

            assert(ColumnCount != 0);

            RowCount = FileCount / ColumnCount;
            if (FileCount % ColumnCount != 0) {
                RowCount += 1;
            }

            //
            // Re-allocate a rectangular array.
            //

            RoundedCount = RowCount * ColumnCount;
            if ((RoundedCount != 0) && (RoundedCount > FileCount)) {
                RoundedFiles = malloc(RoundedCount * sizeof(PLS_FILE));
                if (RoundedFiles == NULL) {
                    return ENOMEM;
                }

                memcpy(RoundedFiles, Files, FileCount * sizeof(PLS_FILE));
                memset(RoundedFiles + FileCount,
                       0,
                       (RoundedCount - FileCount) * sizeof(PLS_FILE));

            } else {
                RoundedFiles = Files;
            }

            SwRotatePointerArray((PVOID *)RoundedFiles, ColumnCount, RowCount);
        }
    }

    //
    // Override the column stuff if there's just one entry per line.
    //

    if ((Context->Flags & LS_OPTION_ONE_ENTRY_PER_LINE) != 0) {
        Context->ColumnCount = 1;
    }

    if (PrintTotal != FALSE) {
        printf("total %lld\n", TotalBlockCount);
    }

    Row = 0;
    ColumnCount = Context->ColumnCount;
    Context->NextColumn = 0;
    for (FileIndex = 0; FileIndex < RoundedCount; FileIndex += 1) {
        File = RoundedFiles[FileIndex];
        if (File == NULL) {
            if ((Context->Flags & LS_OPTION_COLUMN_OUTPUT) != 0) {
                Context->NextColumn = 0;
                Row += 1;
            }

            //
            // Print a newline unless the last one was null too.
            //

            if ((FileIndex == 0) || (RoundedFiles[FileIndex - 1] != NULL)) {
                printf("\n");
            }

            continue;
        }

        LsListFile(Context, File);

        //
        // Print out all the junk that comes after a listing.
        //

        if ((Context->Flags & LS_OPTION_ONE_ENTRY_PER_LINE) != 0) {
            printf("\n");

        } else if ((Context->Flags & LS_OPTION_COLUMN_OUTPUT) != 0) {
            Context->NextColumn += 1;

            //
            // Advance to the next row.
            //

            if ((Context->NextColumn >= ColumnCount) ||
                (FileIndex == RoundedCount - 1)) {

                Context->NextColumn = 0;
                Row += 1;
                printf("\n");

            } else {
                printf("  ");
            }

        } else if ((Context->Flags & LS_OPTION_COMMA_SEPARATED) != 0) {
            if (FileIndex != FileCount - 1) {
                printf(", ");

            } else {
                printf("\n");
            }
        }
    }

    if ((RoundedFiles != NULL) && (RoundedFiles != Files)) {
        free(RoundedFiles);
    }

    return 0;
}

VOID
LsListFile (
    PLS_CONTEXT Context,
    PLS_FILE File
    )

/*++

Routine Description:

    This routine prints out file information.

Arguments:

    Context - Supplies a pointer to the application context.

    File - Supplies a pointer to the file to print.

Return Value:

    None.

--*/

{

    ULONGLONG Number;

    //
    // Print the file number if requested.
    //

    if ((Context->Flags & LS_OPTION_INCLUDE_SERIAL_NUMBERS) != 0) {
        if (File->StatValid != FALSE) {
            Number = File->Stat.st_ino;
            printf("%*llu ", Context->FileNumberColumnSize - 1, Number);

        } else {
            printf("%*s ", Context->FileNumberColumnSize - 1, "?");
        }
    }

    //
    // Print the block count if requested.
    //

    if ((Context->Flags & LS_OPTION_PRINT_BLOCK_COUNT) != 0) {
        if (File->StatValid != FALSE) {
            Number = SwGetBlockCount(&(File->Stat));
            printf("%*llu ", Context->FileBlocksColumnSize - 1, Number);

        } else {
            printf("%*s ", Context->FileBlocksColumnSize - 1, "?");
        }
    }

    //
    // Long format lines look something like this:
    // drwxrwxr-x  3 evan evan  4096 1986-01-08 13:43 testdir
    //

    if ((Context->Flags & LS_OPTION_LONG_FORMAT) != 0) {

        //
        // Print the permissions and the hard link count.
        //

        LsPrintPermissions(File);
        if (File->StatValid != FALSE) {
            Number = File->Stat.st_nlink;
            printf("%*llu ", Context->HardLinkColumnSize - 1, Number);

        } else {
            printf("%*s ", Context->HardLinkColumnSize - 1, "?");
        }

        //
        // Print the user and group if requested.
        //

        if ((Context->Flags & LS_OPTION_SKIP_OWNER) == 0) {
            if (File->OwnerName != NULL) {
                printf("%*s ", Context->OwnerColumnSize, File->OwnerName);

            } else {
                Number = File->Stat.st_uid;
                printf("%*llu ", Context->OwnerColumnSize, Number);
            }
        }

        if ((Context->Flags & LS_OPTION_SKIP_GROUP) == 0) {
            if (File->GroupName != NULL) {
                printf("%*s ", Context->GroupColumnSize, File->GroupName);

            } else {
                Number = File->Stat.st_gid;
                printf("%*llu ", Context->GroupColumnSize, Number);
            }
        }

        //
        // Print the file size.
        //

        if (File->StatValid != FALSE) {
            Number = File->Stat.st_size;
            printf("%*llu ", Context->FileSizeColumnSize, Number);

        } else {
            printf("%*s ", Context->FileSizeColumnSize, "?");
        }

        //
        // Print the date.
        //

        if (File->StatValid != FALSE) {
            if ((Context->Flags & LS_OPTION_USE_ACCESS_TIME) != 0) {
                LsPrintDate(Context, File->Stat.st_atime);

            } else if ((Context->Flags &
                        LS_OPTION_USE_STATUS_CHANGE_TIME) != 0) {

                LsPrintDate(Context, File->Stat.st_ctime);

            } else {
                LsPrintDate(Context, File->Stat.st_mtime);
            }

        } else {
            printf("%*s ", LS_DATE_STRING_SIZE - 1, "?");
        }

        //
        // Finally, print the file name, followed by a newline.
        //

        LsPrintFileName(Context, File);

    } else {

        //
        // Print out the file name, and then maybe a newline or something
        // else.
        //

        LsPrintFileName(Context, File);
    }
}

VOID
LsPrintPermissions (
    PLS_FILE File
    )

/*++

Routine Description:

    This routine prints the standard permissions set.

Arguments:

    File - Supplies a pointer to the file whose permissions should be printed.

Return Value:

    None.

--*/

{

    mode_t Mode;
    CHAR String[13];

    if (File->StatValid == FALSE) {
        printf("??????????  ");
        return;
    }

    memset(String, '-', sizeof(String) - 3);
    String[12] = '\0';

    //
    // This space is just the separator between the permissions and the next
    // field.
    //

    String[11] = ' ';
    Mode = File->Stat.st_mode;

    //
    // Set up the file type.
    //

    if (S_ISDIR(Mode)) {
        String[0] = 'd';

    } else if (S_ISBLK(Mode)) {
        String[0] = 'b';

    } else if (S_ISCHR(Mode)) {
        String[0] = 'c';

    } else if (S_ISLNK(Mode)) {
        String[0] = 'l';

    } else if (S_ISFIFO(Mode)) {
        String[0] = 'p';

    } else if (S_ISSOCK(Mode)) {
        String[0] = 's';
    }

    //
    // Set up the user permissions.
    //

    if ((Mode & S_IRUSR) != 0) {
        String[1] = 'r';
    }

    if ((Mode & S_IWUSR) != 0) {
        String[2] = 'w';
    }

    if ((Mode & S_IXUSR) != 0) {
        String[3] = 'x';
        if ((Mode & S_ISUID) != 0) {
            String[3] = 's';
        }

    } else {
        if ((Mode & S_ISUID) != 0) {
            String[3] = 'S';
        }
    }

    //
    // Set up group permissions.
    //

    if ((Mode & S_IRGRP) != 0) {
        String[4] = 'r';
    }

    if ((Mode & S_IWGRP) != 0) {
        String[5] = 'w';
    }

    if ((Mode & S_IXGRP) != 0) {
        String[6] = 'x';
        if ((Mode & S_ISGID) != 0) {
            String[6] = 's';
        }

    } else {
        if ((Mode & S_ISGID) != 0) {
            String[6] = 'S';
        }
    }

    //
    // Set up other permissions.
    //

    if ((Mode & S_IROTH) != 0) {
        String[7] = 'r';
    }

    if ((Mode & S_IWOTH) != 0) {
        String[8] = 'w';
    }

    if ((Mode & S_IXOTH) != 0) {
        String[9] = 'x';
        if ((Mode & S_ISVTX) != 0) {
            String[9] = 't';
        }

    } else {
        if ((Mode & S_ISVTX) != 0) {
            String[9] = 'T';
        }
    }

    //
    // The "alternate access method" flag is set to a space.
    //

    String[10] = ' ';
    printf("%s", String);
    return;
}

VOID
LsPrintDate (
    PLS_CONTEXT Context,
    time_t Date
    )

/*++

Routine Description:

    This routine prints a file date.

Arguments:

    Context - Supplies a pointer to the application context.

    Date - Supplies the date to print.

Return Value:

    None.

--*/

{

    CHAR Buffer[LS_DATE_STRING_SIZE];
    struct tm CurrentTime;
    time_t CurrentTimeValue;
    struct tm LocalTime;
    struct tm *StaticPointer;
    BOOL Within6Months;

    CurrentTimeValue = time(NULL);
    StaticPointer = localtime(&CurrentTimeValue);
    if (StaticPointer == NULL) {
        printf("%*s ", LS_DATE_STRING_SIZE - 1, "?");
        return;
    }

    memcpy(&CurrentTime, StaticPointer, sizeof(struct tm));
    StaticPointer = localtime(&Date);
    if (StaticPointer == NULL) {
        printf("%*s ", LS_DATE_STRING_SIZE - 1, "?");
        return;
    }

    memcpy(&LocalTime, StaticPointer, sizeof(struct tm));

    //
    // Determine if the timestamp is within six months of the current time. If
    // it's in the future, don't count it.
    //

    Within6Months = TRUE;
    if (Date > CurrentTimeValue) {
        Within6Months = FALSE;

    } else if (LocalTime.tm_year == CurrentTime.tm_year) {
        if (LocalTime.tm_mon + 6 <= CurrentTime.tm_mon) {
            Within6Months = FALSE;
        }

    } else if (LocalTime.tm_year + 1 == CurrentTime.tm_year) {
        if ((int)(LocalTime.tm_mon) - 12 + 6 <= CurrentTime.tm_mon) {
            Within6Months = FALSE;
        }

    } else {
        Within6Months = FALSE;
    }

    //
    // If the timestamp is in the past from the last six months (approximately),
    // then print the date and time. The format for the day of the month should
    // really be %e, but Windows doesn't support that one, so %d is close
    // enough.
    //

    if (Within6Months != FALSE) {
        strftime(Buffer, LS_DATE_STRING_SIZE, "%b %d %H:%M", &LocalTime);

    } else {
        strftime(Buffer, LS_DATE_STRING_SIZE, "%b %d  %Y", &LocalTime);
    }

    printf("%s ", Buffer);
    return;
}

VOID
LsPrintFileName (
    PLS_CONTEXT Context,
    PLS_FILE File
    )

/*++

Routine Description:

    This routine prints the file name.

Arguments:

    Context - Supplies a pointer to the application context.

    File - Supplies a pointer to the file to print.

Return Value:

    None.

--*/

{

    CONSOLE_COLOR Background;
    CHAR Decorator;
    CONSOLE_COLOR Foreground;
    ULONG NameIndex;
    ULONG SpaceIndex;
    ULONG Spaces;

    Background = ConsoleColorDefault;
    Foreground = ConsoleColorDefault;
    Spaces = 0;
    if (Context->NameColumnSize > File->NameSize - 1) {
        Spaces = Context->NameColumnSize - (File->NameSize - 1);
    }

    Decorator = 0;
    NameIndex = 0;
    if ((Context->Flags & LS_OPTION_PRINT_QUESTION_MARKS) != 0) {
        while (File->Name[NameIndex] != '\0') {
            if (isprint(File->Name[NameIndex]) == FALSE) {
                File->Name[NameIndex] = '?';
            }

            NameIndex += 1;
        }
    }

    if ((Context->Flags & LS_OPTION_COLOR) != 0) {
        if (S_ISDIR(File->Stat.st_mode)) {
            Foreground = ConsoleColorBlue;
            if ((File->Stat.st_mode & S_IWOTH) != 0) {
                Background = ConsoleColorGreen;
            }

        } else if (S_ISLNK(File->Stat.st_mode)) {
            if ((Context->Flags & LS_OPTION_ALL_OPERANDS_AS_FILE) == 0) {
                Foreground = ConsoleColorCyan;
                if (File->LinkBroken != FALSE) {
                    Foreground = ConsoleColorRed;
                    Background = ConsoleColorBlack;
                }
            }

        } else if (S_ISSOCK(File->Stat.st_mode)) {
            Foreground = ConsoleColorMagenta;

        } else if ((S_ISBLK(File->Stat.st_mode)) ||
                   (S_ISCHR(File->Stat.st_mode)) ||
                   (S_ISFIFO(File->Stat.st_mode))) {

            Foreground = ConsoleColorYellow;
            Background = ConsoleColorBlack;

        } else if ((File->Stat.st_mode & S_ISUID) != 0) {
            Foreground = ConsoleColorWhite;
            Background = ConsoleColorRed;

        } else if ((File->Stat.st_mode & S_ISGID) != 0) {
            Foreground = ConsoleColorBlack;
            Background = ConsoleColorYellow;

        } else if (File->Stat.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
            Foreground = ConsoleColorGreen;
        }

        if (Foreground != ConsoleColorDefault) {
            SwPrintInColor(Background, Foreground, File->Name);

        } else {
            printf("%s", File->Name);
        }

    } else {
        printf("%s", File->Name);
    }

    if (((Context->Flags & LS_OPTION_DECORATE_DIRECTORIES) != 0) &&
        (S_ISDIR(File->Stat.st_mode))) {

        Decorator = '/';

    } else if (((Context->Flags & LS_OPTION_DECORATE_NAMES) != 0) &&
               ((Context->Flags & LS_OPTION_LONG_FORMAT) == 0)) {

        if (S_ISFIFO(File->Stat.st_mode)) {
            Decorator = '|';

        } else if (S_ISLNK(File->Stat.st_mode)) {
            Decorator = '@';

        } else if ((File->Stat.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0) {
            Decorator = '*';
        }
    }

    if (((Context->Flags & LS_OPTION_LONG_FORMAT) != 0) &&
        ((Context->Flags & LS_OPTION_ALL_OPERANDS_AS_FILE) == 0) &&
        (S_ISLNK(File->Stat.st_mode))) {

        printf(" -> ");
        if (((Context->Flags & LS_OPTION_COLOR) != 0) &&
            (File->LinkBroken != FALSE)) {

            SwPrintInColor(ConsoleColorBlack,
                           ConsoleColorRed,
                           File->LinkDestination);

        } else {
            printf("%s", File->LinkDestination);
        }
    }

    if (Decorator != 0) {
        putchar(Decorator);
        if (Spaces != 0) {
            Spaces -= 1;
        }
    }

    for (SpaceIndex = 0; SpaceIndex < Spaces; SpaceIndex += 1) {
        putchar(' ');
    }

    return;
}

PLS_CONTEXT
LsCreateContext (
    )

/*++

Routine Description:

    This routine creates a new LS context instance.

Arguments:

    None.

Return Value:

    Returns a pointer to the new context on success.

    NULL on allocation failure.

--*/

{

    PLS_CONTEXT Context;

    Context = malloc(sizeof(LS_CONTEXT));
    if (Context == NULL) {
        return NULL;
    }

    memset(Context, 0, sizeof(LS_CONTEXT));

    //
    // Set up some default flags depending on whether or not standard out is a
    // terminal.
    //

    if (isatty(STDOUT_FILENO) != 0) {
        Context->Flags = LS_DEFAULT_OPTIONS_TERMINAL;

    } else {
        Context->Flags = LS_DEFAULT_OPTIONS_NON_TERMINAL;
    }

    return Context;
}

VOID
LsDestroyContext (
    PLS_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates destroys an LS context.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

{

    ULONG FileIndex;

    if (Context->TraversedDirectories != NULL) {
        free(Context->TraversedDirectories);
    }

    if (Context->Files != NULL) {
        for (FileIndex = 0; FileIndex < Context->FilesSize; FileIndex += 1) {
            LsDestroyFileInformation(Context->Files[FileIndex]);
        }

        free(Context->Files);
    }

    if (Context->Directories != NULL) {
        free(Context->Directories);
    }

    free(Context);
    return;
}

PLS_FILE
LsCreateFileInformation (
    PLS_CONTEXT Context,
    PSTR FileName,
    ULONG FileNameSize,
    PSTR LinkDestination,
    ULONG LinkDestinationSize,
    BOOL LinkBroken,
    struct stat *Stat
    )

/*++

Routine Description:

    This routine creates a file information structure.

Arguments:

    Context - Supplies a pointer to the application context.

    FileName - Supplies a pointer to the file name. This string will be copied.

    FileNameSize - Supplies the length of the file buffer in bytes including
        the null terminator.

    LinkDestination - Supplies an optional pointer to the link destination for
        symbolic links, allocated from the heap. This routine will take over
        the allocation, the caller should not subsequently free the memory
        unless this routine fails.

    LinkDestinationSize - Supplies the size of the link destination buffer in
        bytes.

    LinkBroken - Supplies a boolean indicating if the link destination is bad.

    Stat - Supplies a pointer to the file details. If this is NULL, most
        details will be printed as question marks.

Return Value:

    Returns a pointer to a new file information structure on success.

    NULL on allocation failure.

--*/

{

    PLS_FILE NewFile;
    BOOL Result;

    Result = FALSE;
    NewFile = malloc(sizeof(LS_FILE));
    if (NewFile == NULL) {
        goto CreateFileInformationEnd;
    }

    memset(NewFile, 0, sizeof(LS_FILE));

    //
    // Copy the name.
    //

    NewFile->Name = malloc(FileNameSize);
    if (NewFile->Name == NULL) {
        goto CreateFileInformationEnd;
    }

    memcpy(NewFile->Name, FileName, FileNameSize);
    NewFile->Name[FileNameSize - 1] = '\0';
    NewFile->NameSize = FileNameSize;
    if (Stat != NULL) {
        NewFile->StatValid = TRUE;
        memcpy(&(NewFile->Stat), Stat, sizeof(struct stat));
    }

    NewFile->LinkDestination = LinkDestination;
    NewFile->LinkDestinationSize = LinkDestinationSize;
    if (LinkDestination != NULL) {
        NewFile->LinkBroken = LinkBroken;
    }

    //
    // Attempt to get the user and group names if they're going to be needed.
    //

    if (NewFile->StatValid != FALSE) {
        if (((Context->Flags & LS_OPTION_LONG_FORMAT) != 0) &&
            ((Context->Flags & LS_OPTION_PRINT_USER_GROUP_NUMBERS) == 0)) {

            if (SwGetUserNameFromId(Stat->st_uid, &(NewFile->OwnerName)) != 0) {

                assert(NewFile->OwnerName == NULL);
            }

            if (SwGetGroupNameFromId(Stat->st_gid, &(NewFile->GroupName)) !=
                0) {

                assert(NewFile->GroupName == NULL);
            }
        }

    } else {
        NewFile->OwnerName = strdup("?");
        NewFile->GroupName = strdup("?");
    }

    if (NewFile->OwnerName != NULL) {
        NewFile->OwnerNameSize = strlen(NewFile->OwnerName);
    }

    if (NewFile->GroupName != NULL) {
        NewFile->GroupNameSize = strlen(NewFile->GroupName);
    }

    Result = TRUE;

CreateFileInformationEnd:
    if (Result == FALSE) {
        if (NewFile != NULL) {
            if (NewFile->Name != NULL) {
                free(NewFile->Name);
            }

            free(NewFile);
            NewFile = NULL;
        }
    }

    return NewFile;
}

VOID
LsDestroyFileInformation (
    PLS_FILE File
    )

/*++

Routine Description:

    This routine creates destroys an LS file information structure.

Arguments:

    File - Supplies a pointer to the file to destroy.

Return Value:

    None.

--*/

{

    if (File->Name != NULL) {
        free(File->Name);
    }

    if (File->OwnerName != NULL) {
        free(File->OwnerName);
    }

    if (File->GroupName != NULL) {
        free(File->GroupName);
    }

    free(File);
    return;
}

VOID
LsAddTraversedDirectory (
    PLS_CONTEXT Context,
    ino_t Directory
    )

/*++

Routine Description:

    This routine adds an element to the array of traversed directories.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies the serial number of the directory that is about to be
        enumerated.

Return Value:

    None. Failures are silently ignored.

--*/

{

    ino_t *NewBuffer;
    ULONG NewCapacity;

    if ((Context->Flags & LS_OPTION_RECURSIVE) == 0) {
        return;
    }

    if (Directory == 0) {
        return;
    }

    if (Context->TraversedDirectoriesSize >=
        Context->TraversedDirectoriesCapacity) {

        NewCapacity = Context->TraversedDirectoriesCapacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = LS_INITIAL_TRAVERSED_DIRECTORIES_COUNT;
        }

        assert(NewCapacity > Context->TraversedDirectoriesSize);

        NewBuffer = realloc(Context->TraversedDirectories,
                            NewCapacity * sizeof(ino_t));

        if (NewBuffer == NULL) {
            Context->TraversedDirectoriesSize = 0;
            Context->TraversedDirectoriesCapacity = 0;
            return;
        }

        Context->TraversedDirectories = NewBuffer;
        Context->TraversedDirectoriesCapacity = NewCapacity;
    }

    Context->TraversedDirectories[Context->TraversedDirectoriesSize] =
                                                                     Directory;

    Context->TraversedDirectoriesSize += 1;
    return;
}

BOOL
LsHasDirectoryBeenTraversed (
    PLS_CONTEXT Context,
    ino_t Directory
    )

/*++

Routine Description:

    This routine checks to see if the given directory has already been
    traversed.

Arguments:

    Context - Supplies a pointer to the application context.

    Directory - Supplies the serial number of the directory that is about to be
        enumerated.

Return Value:

    TRUE if the directory has already been traversed.

    FALSE if the directory has not been traversed.

--*/

{

    ULONG Index;

    for (Index = 0;
         Index < Context->TraversedDirectoriesSize;
         Index += 1) {

        if (Context->TraversedDirectories[Index] == Directory) {
            return TRUE;
        }
    }

    return FALSE;
}

ULONG
LsGetCharacterCountForInteger (
    ULONGLONG Integer
    )

/*++

Routine Description:

    This routine returns the number of characters required to represent the
    given unsigned integer.

Arguments:

    Integer - Supplies the integer that will get printed.

Return Value:

    Returns the number of characters required to print a string of that
    decimal integer.

--*/

{

    ULONGLONG Comparison;
    ULONG Count;

    Count = 1;
    Comparison = 10;
    while ((Count < 24) && (Integer >= Comparison)) {
        Count += 1;
        Comparison *= 10;
    }

    return Count;
}


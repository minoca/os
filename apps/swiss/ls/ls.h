/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ls.h

Abstract:

    This header contains definitions for the ls utility.

Author:

    Evan Green 25-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define LS options.
//

//
// Set this option to display multi-text-column output.
//

#define LS_OPTION_COLUMN_OUTPUT 0x00000001

//
// Set this option to follow symbolic links to directories in operands.
//

#define LS_OPTION_FOLLOW_LINKS_IN_OPERANDS 0x00000002

//
// Set this option to decorate the names (/ for directory, * for executable,
// | for FIFOs, and @ for sym links).
//

#define LS_OPTION_DECORATE_NAMES 0x00000004

//
// Set this option to follow links in files found.
//

#define LS_OPTION_FOLLOW_LINKS_IN_LIST 0x00000008

//
// Set this option to recurse into subdirectories.
//

#define LS_OPTION_RECURSIVE 0x00000010

//
// Set this option to include names that begin with a period.
//

#define LS_OPTION_LIST_ALL 0x00000020

//
// Set this option to use status change time instead of modification time.
//

#define LS_OPTION_USE_STATUS_CHANGE_TIME 0x00000040

//
// Set this option to avoid treating directories different than files on the
// command line.
//

#define LS_OPTION_ALL_OPERANDS_AS_FILE 0x00000080

//
// Set this option to diable sorting.
//

#define LS_OPTION_NO_SORTING 0x00000100

//
// Set this option to skip printing the owner.
//

#define LS_OPTION_SKIP_OWNER 0x00000200

//
// Set this option to print file serial numbers.
//

#define LS_OPTION_INCLUDE_SERIAL_NUMBERS 0x00000400

//
// Set this option to display entries in long format.
//

#define LS_OPTION_LONG_FORMAT 0x00000800

//
// Set this option to list the files as a comma-separated list.
//

#define LS_OPTION_COMMA_SEPARATED 0x00001000

//
// Set this option to write out the owner and group as a number rather than the
// associated string.
//

#define LS_OPTION_PRINT_USER_GROUP_NUMBERS 0x00002000

//
// Set this option to skip printing the group.
//

#define LS_OPTION_SKIP_GROUP 0x00004000

//
// Set this option to append a slash to all directories.
//

#define LS_OPTION_DECORATE_DIRECTORIES 0x00008000

//
// Set this option to print all non-printable characters and tabs as
// question marks.
//

#define LS_OPTION_PRINT_QUESTION_MARKS 0x00010000

//
// Set this option to reverse the sort order.
//

#define LS_OPTION_REVERSE_SORT 0x00020000

//
// Set this option to print the block count.
//

#define LS_OPTION_PRINT_BLOCK_COUNT 0x00040000

//
// Set this option to sort by the modification (or creation or access) date,
// with a secondary key being the file name.
//

#define LS_OPTION_SORT_BY_DATE 0x00080000

//
// Set this option to use the last access time rather than the modification
// time.
//

#define LS_OPTION_USE_ACCESS_TIME 0x00100000

//
// Set this entry to sort column-based output across rather than down.
//

#define LS_OPTION_SORT_COLUMNS_ACROSS 0x00200000

//
// Set this option to print one entry per line.
//

#define LS_OPTION_ONE_ENTRY_PER_LINE 0x00400000

//
// Set this option to print in color.
//

#define LS_OPTION_COLOR 0x00800000

//
// Set this internal flag to print the directory names before their contents.
//

#define LS_OPTION_PRINT_DIRECTORY_NAME 0x01000000

//
// Define the default flags for output to a terminal and output to a
// non-terminal.
//

#define LS_DEFAULT_OPTIONS_TERMINAL                             \
    (LS_OPTION_COLUMN_OUTPUT | LS_OPTION_PRINT_QUESTION_MARKS | LS_OPTION_COLOR)

#define LS_DEFAULT_OPTIONS_NON_TERMINAL LS_OPTION_ONE_ENTRY_PER_LINE

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines information about a single file.

Members:

    Name - Stores a pointer to the name of the file, allocated from the heap.

    NameSize - Stores the size of the name string buffer in bytes including the
        null terminator.

    LinkDestination - Stores the destination if this is a link.

    LinkDestinationSize - Stores the size of the link destination string in
        bytes including the null terminator.

    LinkBroken - Stores a boolean indicating that the symbolic link is broken.

    OwnerName - Stores an optional pointer to a string containing the name of
        the user that owns this file.

    OwnerNameSize - Stores the size of the owner name buffer in bytes
        including the null terminator.

    GroupName - Stores an optional pointer to a string containing the name
        of the group that owns this file.

    GroupNameSize - Stores the size of the group name buffer in bytes
        including the null terminator.

    Stat - Stores the file information.

    StatValid - Stores a boolean indicating if the stat information is valid.

--*/

typedef struct _LS_FILE {
    PSTR Name;
    ULONG NameSize;
    PSTR LinkDestination;
    ULONG LinkDestinationSize;
    BOOL LinkBroken;
    PSTR OwnerName;
    ULONG OwnerNameSize;
    PSTR GroupName;
    ULONG GroupNameSize;
    struct stat Stat;
    BOOL StatValid;
} LS_FILE, *PLS_FILE;

/*++

Structure Description:

    This structure defines the context for an instance of the LS application.

Members:

    Flags - Stores the options for this instance. See LS_OPTION_* definitions.

    ArgumentsPrinted - Stores the number of arguments printed so far.

    ItemsPrinted - Stores the number of elements printed so far.

    Files - Stores the array of strings containing the individual files to list.

    FilesSize - Stores the number of valid elements in the files array.

    FilesCapacity - Stores the maximum size of the files array in elements.

    Directories - Stores the array of strings containing the individual
        directories to list.

    DirectoresSize - Stores the number of valid elements in the directories
        array.

    DirectoriesCapacity - Stores the maximum size of the directories array in
        elements.

    TraversedDirectories - Stores an array of file serial numbers for
        directories that have already been traversed. This is used for
        recursion loop detection.

    TraversedDirectoriesSize - Stores the number of elements in the traversed
        directories array.

    TraversedDirectoriesCapacity - Stores the capacity of the traversed
        directories allocation.

    NameColumnSize - Stores the size of a column for the file name in
        column-based output.

    ColumnCount - Stores the number of columns to print per line.

    NextColumn - Stores the index of the next column to be printed.

    FileNumberColumnSize - Stores the size of the column for file numbers in
        column-based output.

    FileBlocksColumnSize - Stores the size of the column for file blocks in
        column-based output.

    FileSizeColumnSize - Stores the size of the column containing the file
        size.

    HardLinkColumnSize - Stores the size of the hard link column.

    OwnerColumnSize - Stores the size of the owner column.

    GroupColumnSize - Stores the size of the group column.

--*/

typedef struct _LS_CONTEXT {
    ULONG Flags;
    ULONG ArgumentsPrinted;
    ULONG ItemsPrinted;
    PLS_FILE *Files;
    ULONG FilesSize;
    ULONG FilesCapacity;
    PSTR *Directories;
    ULONG DirectoriesSize;
    ULONG DirectoriesCapacity;
    ino_t *TraversedDirectories;
    ULONG TraversedDirectoriesSize;
    ULONG TraversedDirectoriesCapacity;
    ULONG NameColumnSize;
    ULONG ColumnCount;
    ULONG NextColumn;
    ULONG FileNumberColumnSize;
    ULONG FileBlocksColumnSize;
    ULONG FileSizeColumnSize;
    ULONG HardLinkColumnSize;
    ULONG OwnerColumnSize;
    ULONG GroupColumnSize;
} LS_CONTEXT, *PLS_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Sorting compare functions
//

int
LsCompareFilesByName (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by file name.

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

int
LsCompareFilesByReverseName (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by file name, in reverse.

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

int
LsCompareFilesByModificationDate (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by modification date.

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

int
LsCompareFilesByReverseModificationDate (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by reverse modification date.

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

int
LsCompareFilesByStatusChangeDate (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by status change date.

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

int
LsCompareFilesByReverseStatusChangeDate (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by reverse status change date.

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

int
LsCompareFilesByAccessDate (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by its last access date.

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

int
LsCompareFilesByReverseAccessDate (
    const void *Item1,
    const void *Item2
    );

/*++

Routine Description:

    This routine compares two files by reverse access date.

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

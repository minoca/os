/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    find.c

Abstract:

    This module implements the find utility.

Author:

    Evan Green 29-Aug-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define FIND_VERSION_MAJOR 1
#define FIND_VERSION_MINOR 0

#define FIND_USAGE \
    "usage: find [-H | -L] path... [operand_expression...]\n\n"                \
    "The find utility recursively searches through each path specified for \n" \
    "file names matching the given operand expression. Options are:\n"         \
    "  -H -- Follow symbolic links only in path operands. If the link \n"      \
    "        destinations do not exist and for all paths not specified in \n"  \
    "        the command line, use the link itself.\n"                         \
    "  -L -- Always follow symbolic links.\n\n"                                \
    "Where n is used below in expression primary operands, n is a decimal.\n"  \
    "integer. If preceded with a +, the expression is \"greater than n\".\n"   \
    "If preceded with a -, the expression is \"less than n\".\n"               \
    "Operand expression primaries are:\n"                                      \
    "  -name pattern -- Evaluates to true if the file name matches the \n"     \
    "        given pattern (same pattern rules as the shell).\n"               \
    "  -nouser -- Evaluates to true if getpwuid for the user returns NULL.\n"  \
    "  -nogroup -- Evalueates to true if getgrgid for the group returns "      \
    "NULL.\n"                                                                  \
    "  -xdev -- Evaluates to true, and causes the entire expression not to \n" \
    "        descend into another device.\n"                                   \
    "  -prune -- Evaluates to true, and does not descend if the current \n"    \
    "        path is a directory. This is ignored if -depth is on.\n"          \
    "  -perm [-]mode -- Takes in a mode (starting with an empty mask) in \n"   \
    "        the same format as chmod. If no hyphen was specified, it \n"      \
    "        evaluates to true if the permissions match exactly. If a \n"      \
    "        hyphen was specified, evaluates to true if at least the given \n" \
    "        permission bits are set.\n"                                       \
    "  -type c -- Evaluates to true if the current file type matches. Valid \n"\
    "        values for c are bcdlpfs (block, character, directory, link,\n"   \
    "        pipe, normal file, socket).\n"                                    \
    "  -links n -- Evaluates to true if the file has n links.\n"               \
    "  -user uname -- Evalues to true if the file is owned by the given \n"    \
    "        user, which can be a user name or ID.\n"                          \
    "  -uid n -- Evaluates to true if the user ID matches.\n"                  \
    "  -gid n -- Evaluates to true if the group ID matches.\n"                 \
    "  -group gname -- Evaluates to true if the file is owned by the given \n" \
    "        group, which can be a group name or ID.\n"                        \
    "  -size n[c] -- Evaluates to true if the file size divided by 512 and \n" \
    "        rounded up is n. If the c is present, the file size is \n"        \
    "        evaluated in bytes.\n"                                            \
    "  -atime n -- Evaluates to true if the file access time minus the \n"     \
    "        current time divided by 86400 seconds (one day) is n.\n"          \
    "  -mtime n -- Evaluates to true if the file modification time minus \n"   \
    "        the current time divided by 86400 is n.\n"                        \
    "  -ctime n -- Evaluates to true if the file status change time minus \n"  \
    "        the current time divided by 86400 is n.\n"                        \
    "  -exec utility [argument...]; -- Executes the given utility and \n"      \
    "        evaluates to true if the utility returns 0. Instances of {} \n"   \
    "        found in the arguments (not necessarily alone in an argument) \n" \
    "        are replaced with the current file path.\n"                       \
    "  -exec utility [argument...] {} + -- Always evaluates to true. \n"       \
    "        Executes the given utility, batching together multiple \n"        \
    "        matching file arguments, which will all be added as separate \n"  \
    "        arguments at the end of the command line. Only the required {} \n"\
    "        at the end is replaced with the arguments, other instances of \n" \
    "        {} are ignored.\n"                                                \
    "  -ok utility [argument...] -- Works the same as the semicolon-\n"        \
    "        delimited version of exec, but prompts the user via stderr to \n" \
    "        execute each instantiation of the utility. Evaluates to false \n" \
    "        if the user says no.\n"                                           \
    "  -print -- Evaluates to true, prints the current file name.\n"           \
    "  -newer file -- Evaluates to true if the modification time of the \n"    \
    "        current file is newer than that of the given file.\n"             \
    "  -depth -- Evaluates to true. All entries in a directory are acted on \n"\
    "        before the directory itself.\n"                                   \
    "  -true -- Always evaluates to true.\n"                                   \
    "  -false -- Always evaluates to false.\n\n"                               \
    "Expressions can be combined in the forms:\n"                              \
    "  ( expression ) -- Grouping\n"                                           \
    "  ! expression -- Negation\n"                                             \
    "  expression [-a] expression -- Logical and\n"                            \
    "  expression -o expression -- Logical or\n\n"                             \
    "Remember to escape characters like !, (, and ), as they are recognized \n"\
    "by the shell. If no expression is present, -print is used. If none of \n" \
    "-exec, -ok, or -print are present, -print is added to the end of the \n"  \
    "expression.\n"                                                            \
    "Returns 0 if all paths were traversed successfully, or >0 if an error \n" \
    "occurred.\n\n"

//
// Define the global application options.
//

//
// Set this option to follow links found in command line arguments only. If the
// link is not valid or for links found in traversal, use the value of the link
// itself.
//

#define FIND_OPTION_LINKS_IN_OPERANDS 0x00000001

//
// Set this option to use the desintation for any symbolic link found.
//

#define FIND_OPTION_FOLLOW_ALL_LINKS  0x00000002

//
// Set this option to skip traversals to devices different than the intial
// path.
//

#define FIND_OPTION_NO_CROSS_DEVICE 0x00000004

//
// Set this option to process the contents of any directories before the
// directory itself.
//

#define FIND_OPTION_DEPTH_FIRST 0x00000008

//
// Set this option if there is an implied print at the end of every evaluation.
// This is set automatically if no -exec, -print, or -ok is in any of the
// arguments.
//

#define FIND_OPTION_IMPLIED_PRINT 0x00000010

//
// This mask combines either of the two link following options.
//

#define FIND_OPTION_LINK_MASK \
    (FIND_OPTION_LINKS_IN_OPERANDS | FIND_OPTION_FOLLOW_ALL_LINKS)

//
// Define the initial element count in the searched directory array.
//

#define FIND_SEARCHED_DIRECTORY_INITIAL_SIZE 16

//
// Define the permissions mask find pays attention to during permission
// checks.
//

#define FIND_PERMISSIONS_MASK \
    (S_IRWXO | S_IRWXG | S_IRWXU | S_ISUID | S_ISGID | S_ISVTX)

//
// Define the block size used by the file size test.
//

#define FIND_FILE_BLOCK_SIZE 512

//
// Define the factor that file times are divided by to get to the values passed
// on the command line.
//

#define SECONDS_PER_DAY 86400

//
// Define the maximum number of file paths that are batched up before calling
// a batched exec.
//

#define FIND_BATCH_SIZE 15

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _FIND_NODE_TYPE {
    FindNodeInvalid,
    FindNodeParentheses,
    FindNodeOr,
    FindNodePrint,
    FindNodeName,
    FindNodeNoUser,
    FindNodeNoGroup,
    FindNodeTrue,
    FindNodeFalse,
    FindNodePrune,
    FindNodePermissions,
    FindNodeFileType,
    FindNodeLinks,
    FindNodeUserName,
    FindNodeGroupName,
    FindNodeUserId,
    FindNodeGroupId,
    FindNodeSize,
    FindNodeAccessTime,
    FindNodeStatusChangeTime,
    FindNodeModificationTime,
    FindNodeExecute,
    FindNodeNewer,
    FindNodeCount
} FIND_NODE_TYPE, *PFIND_NODE_TYPE;

typedef enum _FIND_INTEGER_TYPE {
    FindIntegerInvalid,
    FindIntegerLessThan,
    FindIntegerEqualTo,
    FindIntegerGreaterThan
} FIND_INTEGER_TYPE, *PFIND_INTEGER_TYPE;

/*++

Structure Description:

    This structure defines the contents of a file name find node.

Members:

    Name - Stores a pointer to the file name pattern.

    PatternSize - Stores the size of the pattern in bytes including the null
        terminator.

--*/

typedef struct _FIND_NODE_NAME {
    PSTR Pattern;
    UINTN PatternSize;
} FIND_NODE_NAME, *PFIND_NODE_NAME;

/*++

Structure Description:

    This structure defines an integer for a find node parameter.

Members:

    Type - Stores the comparison type for this integer.

    Value - Stores the value of the integer.

--*/

typedef struct _FIND_NODE_INTEGER {
    FIND_INTEGER_TYPE Type;
    LONG Value;
} FIND_NODE_INTEGER, *PFIND_NODE_INTEGER;

/*++

Structure Description:

    This structure defines the contents of a permissions find node.

Members:

    Permissions - Stores the specified permissions bits.

    Exact - Stores a boolean indicating whether the permissions need to match
        exactly, or only at least have these bits set.

--*/

typedef struct _FIND_NODE_PERMISSIONS {
    mode_t Permissions;
    BOOL Exact;
} FIND_NODE_PERMISSIONS, *PFIND_NODE_PERMISSIONS;

/*++

Structure Description:

    This structure defines the contents of a file size find node.

Members:

    Integer - Stores the integer size value.

    Bytes - Stores a boolean indicating if the size is to be measured in bytes
        (TRUE) or if the file size is to be measured in 512-byte blocks (FALSE).

--*/

typedef struct _FIND_NODE_SIZE {
    FIND_NODE_INTEGER Integer;
    BOOL Bytes;
} FIND_NODE_SIZE, *PFIND_NODE_SIZE;

/*++

Structure Description:

    This structure defines the contents of an execute find node.

Members:

    Argument - Stores a pointer to the array of arguments, starting with the
        utility name.

    ArgumentCount - Stores the number of arguments, which must be at least 1.

    Confirm - Stores a boolean indicating if the user should be prompted before
        each execution (an -ok predicate).

    Batch - Stores a boolean indicating if the results should be batched (the
        -exec predicate was terminated in a +).

    NewArguments - Stores an array of the new arguments that exec will be
        called with. This will be at least as large as the above argument count,
        plus the maximum batch size for batched executes.

    BatchCount - Stores the number of files currently batched in the arguments.

--*/

typedef struct _FIND_NODE_EXECUTE {
    PSTR *Arguments;
    ULONG ArgumentCount;
    BOOL Confirm;
    BOOL Batch;
    PSTR *NewArguments;
    ULONG BatchCount;
} FIND_NODE_EXECUTE, *PFIND_NODE_EXECUTE;

typedef struct _FIND_NODE FIND_NODE, *PFIND_NODE;

/*++

Structure Description:

    This structure defines a find expression node.

Members:

    ListEntry - Stores pointers to the next and previous nodes in the logically
        anded expression list.

    Parent - Stores a pointer to the parent expression.

    Type - Stores the type of node this is.

    Negate - Stores a boolean indicating if the result of this node should be
        negated.

    ChildList - Stores the list of child nodes for a group node.

    Name - Stores a pointer to the name for a name pattern for a name node.

    Permissions - Stores the permissions information for a permissions node.

    FileType - Stores the file type character for file type nodes.

    Integer - Stores the integer operand for those predicates that take a single
        integer.

    Size - Stores the file size information for size nodes.

    ModificationTime - Stores the modification time to be newer than for find
        "newer" nodes.

    Execute - Stores information about the execution of a utility for execute
        nodes.

--*/

struct _FIND_NODE {
    LIST_ENTRY ListEntry;
    PFIND_NODE Parent;
    FIND_NODE_TYPE Type;
    BOOL Negate;
    union {
        LIST_ENTRY ChildList;
        FIND_NODE_NAME Name;
        FIND_NODE_PERMISSIONS Permissions;
        CHAR FileType;
        FIND_NODE_INTEGER Integer;
        FIND_NODE_SIZE Size;
        time_t ModificationTime;
        FIND_NODE_EXECUTE Execute;
    } U;

};

/*++

Structure Description:

    This structure defines a visit to a directory.

Members:

    Device - Stores the device number.

    FileNumber - Stores the file serial number.

--*/

typedef struct _FIND_VISIT {
    dev_t Device;
    ino_t FileNumber;
} FIND_VISIT, *PFIND_VISIT;

/*++

Structure Description:

    This structure defines the context for an instance of the find application.

Members:

    HeadNode - Stores the head node of the expression, which is a fake
        parentheses group that contains all the other nodes.

    InputIndex - Stores the first index of input paths.

    InputCount - Stores the count of input paths.

    Options - Stores a bitfield of application options.

    SearchedDirectories - Stores an array of directories that have been
        visited.

    SearchedDirectoryCount - Stores the number of valid elements in the
        searched directory array.

    SearchedDirectoryCapacity - Stores the maximum number of elements the
        searched directories array can hold before it has to be reallocated.

    RootDevice - Stores the device of the command line input path.

    CurrentTime - Stores the time at instantiation.

--*/

typedef struct _FIND_CONTEXT {
    FIND_NODE HeadNode;
    INT InputIndex;
    INT InputCount;
    ULONG Options;
    PFIND_VISIT SearchedDirectories;
    ULONG SearchedDirectoryCount;
    ULONG SearchedDirectoryCapacity;
    dev_t RootDevice;
    time_t CurrentTime;
} FIND_CONTEXT, *PFIND_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
FindExecuteSearch (
    PFIND_CONTEXT Context,
    PSTR Path,
    BOOL FromCommandLine
    );

INT
FindTestFile (
    PFIND_CONTEXT Context,
    PSTR Path,
    struct stat *Stat,
    PBOOL Prune
    );

INT
FindEvaluateNode (
    PFIND_CONTEXT Context,
    PSTR Path,
    struct stat *Stat,
    PFIND_NODE Node,
    PBOOL Match,
    PBOOL Prune
    );

INT
FindEvaluateExecute (
    PFIND_CONTEXT Context,
    PSTR Path,
    PFIND_NODE Node,
    PBOOL Match
    );

INT
FindExecute (
    PSTR *Arguments,
    INT ArgumentCount,
    PINT ReturnValue
    );

PSTR
FindSubstitutePath (
    PSTR Argument,
    PSTR Path
    );

BOOL
FindEvaluateIntegerTest (
    PFIND_NODE_INTEGER Integer,
    LONGLONG Value
    );

INT
FindFlushBatchExecutes (
    PFIND_CONTEXT Context,
    PFIND_NODE Node
    );

INT
FindParseArguments (
    PFIND_CONTEXT Context,
    INT ArgumentCount,
    CHAR **Arguments
    );

INT
FindParseNode (
    PFIND_CONTEXT Context,
    PFIND_NODE Parent,
    PSTR *Arguments,
    INT ArgumentCount,
    PINT ArgumentIndex,
    PFIND_NODE *CreatedNode
    );

INT
FindParseInteger (
    PSTR Argument,
    PFIND_NODE_INTEGER Integer
    );

PFIND_NODE
FindCreateNode (
    FIND_NODE_TYPE Type,
    PFIND_NODE Parent
    );

VOID
FindDestroyNode (
    PFIND_NODE Node
    );

INT
FindAddSearchedDirectory (
    PFIND_CONTEXT Context,
    dev_t Device,
    ino_t FileNumber,
    PBOOL AlreadyVisited
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
FindMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the find utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    PFIND_NODE Child;
    FIND_CONTEXT Context;
    INT Result;

    if ((ArgumentCount > 1) && (strcmp(Arguments[1], "--version") == 0)) {
        SwPrintVersion(FIND_VERSION_MAJOR, FIND_VERSION_MINOR);
        return 1;
    }

    if ((ArgumentCount > 1) && (strcmp(Arguments[1], "--help") == 0)) {
        printf(FIND_USAGE);
        return 1;
    }

    memset(&Context, 0, sizeof(FIND_CONTEXT));
    INITIALIZE_LIST_HEAD(&(Context.HeadNode.U.ChildList));
    Context.CurrentTime = time(NULL);
    Result = FindParseArguments(&Context, ArgumentCount, Arguments);
    if (Result != 0) {
        goto FindMainEnd;
    }

    if (Context.InputCount == 0) {
        Context.RootDevice = -1;
        Result = FindExecuteSearch(&Context, ".", TRUE);
        if (Result != 0) {
            goto FindMainEnd;
        }

        Result = FindFlushBatchExecutes(&Context, &(Context.HeadNode));
        if (Result != 0) {
            goto FindMainEnd;
        }

    } else {
        for (ArgumentIndex = Context.InputIndex;
             ArgumentIndex < Context.InputIndex + Context.InputCount;
             ArgumentIndex += 1) {

            Argument = Arguments[ArgumentIndex];
            Context.RootDevice = -1;

            assert(Context.SearchedDirectoryCount == 0);

            Result = FindExecuteSearch(&Context, Argument, TRUE);
            if (Result != 0) {
                goto FindMainEnd;
            }

            Result = FindFlushBatchExecutes(&Context, &(Context.HeadNode));
            if (Result != 0) {
                goto FindMainEnd;
            }
        }
    }

FindMainEnd:

    //
    // Destroy any children of the head node.
    //

    while (LIST_EMPTY(&(Context.HeadNode.U.ChildList)) == FALSE) {
        Child = LIST_VALUE(Context.HeadNode.U.ChildList.Next,
                           FIND_NODE,
                           ListEntry);

        FindDestroyNode(Child);
    }

    if (Context.SearchedDirectories != NULL) {
        free(Context.SearchedDirectories);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
FindExecuteSearch (
    PFIND_CONTEXT Context,
    PSTR Path,
    BOOL FromCommandLine
    )

/*++

Routine Description:

    This routine executes a find operation on the given directory.

Arguments:

    Context - Supplies the application context.

    Path - Supplies a pointer to the string containing the path to search.

    FromCommandLine - Supplies a boolean indicating whether this search is
        coming directly from the command line (TRUE) or from a recursion
        (FALSE).

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    BOOL AlreadyVisited;
    PSTR AppendedPath;
    ULONG AppendedPathSize;
    DIR *Directory;
    struct dirent *Entry;
    BOOL FollowLinks;
    BOOL Prune;
    INT Result;
    struct stat Stat;
    BOOL VisitAdded;

    AppendedPath = NULL;
    Directory = NULL;
    VisitAdded = FALSE;
    FollowLinks = FALSE;
    if (FromCommandLine != FALSE) {
        if ((Context->Options & FIND_OPTION_LINK_MASK) != 0) {
            FollowLinks = TRUE;
        }

    } else {
        if ((Context->Options & FIND_OPTION_FOLLOW_ALL_LINKS) != 0) {
            FollowLinks = TRUE;
        }
    }

    Result = SwStat(Path, FollowLinks, &Stat);
    if (Result != 0) {
        SwPrintError(Result, Path, "Unable to stat");
        goto FindExecuteSearchEnd;
    }

    //
    // Set the root device if it hasn't been set, or avoid crossing devices if
    // that option is on.
    //

    if (Context->RootDevice == -1) {
        Context->RootDevice = Stat.st_dev;

    } else if (((Context->Options & FIND_OPTION_NO_CROSS_DEVICE) != 0) &&
               (Stat.st_dev != Context->RootDevice)) {

        Result = 0;
        goto FindExecuteSearchEnd;
    }

    //
    // If it's a directory, add it to the list for loop detection.
    //

    if (S_ISDIR(Stat.st_mode)) {
        Result = FindAddSearchedDirectory(Context,
                                          Stat.st_dev,
                                          Stat.st_ino,
                                          &AlreadyVisited);

        if (Result != 0) {
            goto FindExecuteSearchEnd;
        }

        if (AlreadyVisited != FALSE) {
            SwPrintError(0, Path, "Skipping previously visited directory");
            Result = 0;
            goto FindExecuteSearchEnd;
        }

        VisitAdded = TRUE;
    }

    //
    // If it's not a directory or depth first is off, evaluate the path now.
    //

    if ((!S_ISDIR(Stat.st_mode)) ||
        ((Context->Options & FIND_OPTION_DEPTH_FIRST) == 0)) {

        Prune = FALSE;
        Result = FindTestFile(Context, Path, &Stat, &Prune);
        if ((Result != 0) || (!S_ISDIR(Stat.st_mode))) {
            goto FindExecuteSearchEnd;
        }

        //
        // If it's a directory and it got pruned, don't go inside.
        //

        if ((S_ISDIR(Stat.st_mode)) && (Prune != FALSE)) {
            goto FindExecuteSearchEnd;
        }
    }

    //
    // This is a directory. Crack it open and recurse into the entries.
    //

    Directory = opendir(Path);
    if (Directory == NULL) {
        Result = errno;
        SwPrintError(Result, Path, "Unable to open directory");
        goto FindExecuteSearchEnd;
    }

    //
    // Loop through all entries in the directory.
    //

    while (TRUE) {
        errno = 0;
        Entry = readdir(Directory);
        if (Entry == NULL) {
            Result = errno;
            if (Result != 0) {
                SwPrintError(Result, Path, "Unable to read directory");
                goto FindExecuteSearchEnd;
            }

            break;
        }

        if ((strcmp(Entry->d_name, ".") == 0) ||
            (strcmp(Entry->d_name, "..") == 0)) {

            continue;
        }

        Result = SwAppendPath(Path,
                              strlen(Path) + 1,
                              Entry->d_name,
                              strlen(Entry->d_name) + 1,
                              &AppendedPath,
                              &AppendedPathSize);

        if (Result == FALSE) {
            Result = ENOMEM;
            goto FindExecuteSearchEnd;
        }

        Result = FindExecuteSearch(Context, AppendedPath, FALSE);
        free(AppendedPath);
        if (Result != 0) {
            goto FindExecuteSearchEnd;
        }
    }

    //
    // If this node is a directory and the depth first flag is on, then perform
    // the deferred evaluation of the directory itself now.
    //

    if ((Context->Options & FIND_OPTION_DEPTH_FIRST) != 0) {
        Result = FindTestFile(Context, Path, &Stat, &Prune);
        if (Result != 0) {
            goto FindExecuteSearchEnd;
        }
    }

FindExecuteSearchEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    //
    // Pop the visit off if one was added.
    //

    if (VisitAdded != FALSE) {

        assert(Context->SearchedDirectoryCount != 0);

        Context->SearchedDirectoryCount -= 1;
    }

    return Result;
}

INT
FindTestFile (
    PFIND_CONTEXT Context,
    PSTR Path,
    struct stat *Stat,
    PBOOL Prune
    )

/*++

Routine Description:

    This routine executes the file test for the given file.

Arguments:

    Context - Supplies the application context.

    Path - Supplies a pointer to the file name. This must not be modified.

    Stat - Supplies a pointer to the stat structure for the file.

    Prune - Supplies a pointer to a boolean indicating whether or not a prune
        primary was hit.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    BOOL Match;
    INT Result;

    *Prune = FALSE;
    Result = FindEvaluateNode(Context,
                              Path,
                              Stat,
                              &(Context->HeadNode),
                              &Match,
                              Prune);

    if (Result != 0) {
        return Result;
    }

    if ((Context->Options & FIND_OPTION_IMPLIED_PRINT) != 0) {
        if (Match != FALSE) {
            printf("%s\n", Path);
        }
    }

    return 0;
}

INT
FindEvaluateNode (
    PFIND_CONTEXT Context,
    PSTR Path,
    struct stat *Stat,
    PFIND_NODE Node,
    PBOOL Match,
    PBOOL Prune
    )

/*++

Routine Description:

    This routine executes the file test for the given file.

Arguments:

    Context - Supplies the application context.

    Path - Supplies a pointer to the file name. This must not be modified.

    Stat - Supplies a pointer to the stat structure for the file.

    Node - Supplies a pointer to the node to evaluate.

    Match - Supplies a pointer where a boolean will be returned indicating
        whether the given file passed the test.

    Prune - Supplies a pointer to a boolean indicating whether or not a prune
        primary was hit.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR BaseName;
    PFIND_NODE Child;
    PLIST_ENTRY CurrentEntry;
    PSTR PathCopy;
    INT Result;
    size_t Size;
    time_t Time;

    Result = 0;
    *Match = FALSE;
    switch (Node->Type) {

    //
    // For parentheses, loop through and evaluate the children. There's an
    // implied AND between each node (except when an OR is found).
    //

    case FindNodeParentheses:
        CurrentEntry = Node->U.ChildList.Next;
        while (CurrentEntry != &(Node->U.ChildList)) {
            Child = LIST_VALUE(CurrentEntry, FIND_NODE, ListEntry);
            if (Child->Type == FindNodeOr) {

                //
                // If the left side is true, then stop processing. Otherwise,
                // skip this node and start evaluating from the right side.
                //

                if (*Match != FALSE) {
                    break;
                }

            } else {
                Result = FindEvaluateNode(Context,
                                          Path,
                                          Stat,
                                          Child,
                                          Match,
                                          Prune);

                if (Result != 0) {
                    return Result;
                }

                //
                // If the match did not succeed, scan ahead looking for the
                // next OR statement.
                //

                if (*Match == FALSE) {
                    CurrentEntry = CurrentEntry->Next;
                    while (CurrentEntry != &(Node->U.ChildList)) {
                        Child = LIST_VALUE(CurrentEntry, FIND_NODE, ListEntry);
                        if (Child->Type == FindNodeOr) {
                            break;
                        }

                        CurrentEntry = CurrentEntry->Next;
                    }

                    //
                    // Continue without advancing again, since the loop above
                    // did all the advancing.
                    //

                    continue;
                }
            }

            CurrentEntry = CurrentEntry->Next;
        }

        break;

    case FindNodePrint:
        printf("%s\n", Path);
        *Match = TRUE;
        break;

    case FindNodeName:
        PathCopy = strdup(Path);
        if (PathCopy == NULL) {
            return ENOMEM;
        }

        BaseName = basename(PathCopy);
        if (BaseName == NULL) {
            Result = errno;
            SwPrintError(Result, PathCopy, "Basename failed");
            if (Result == 0) {
                Result = EINVAL;
            }

            free(PathCopy);
            goto EvaluateNodeEnd;
        }

        *Match = SwDoesPatternMatch(BaseName,
                                    strlen(BaseName) + 1,
                                    Node->U.Name.Pattern,
                                    Node->U.Name.PatternSize);

        break;

    case FindNodeNoUser:
        *Match = TRUE;
        Result = SwGetUserNameFromId(Stat->st_uid, NULL);
        if (Result == 0) {
            *Match = FALSE;
        }

        Result = 0;
        break;

    case FindNodeNoGroup:
        *Match = TRUE;
        Result = SwGetGroupNameFromId(Stat->st_gid, NULL);
        if (Result == 0) {
            *Match = FALSE;
        }

        Result = 0;
        break;

    case FindNodeTrue:
        *Match = TRUE;
        break;

    case FindNodeFalse:
        *Match = FALSE;
        break;

    case FindNodePrune:
        *Match = TRUE;
        *Prune = TRUE;
        break;

    case FindNodePermissions:
        if (Node->U.Permissions.Exact != FALSE) {
            if ((Stat->st_mode & FIND_PERMISSIONS_MASK) ==
                Node->U.Permissions.Permissions) {

                *Match = TRUE;
            }

        //
        // If at least the given bits are set, then it's a match.
        //

        } else if ((Stat->st_mode & Node->U.Permissions.Permissions) ==
                   Node->U.Permissions.Permissions) {

            *Match = TRUE;
        }

        break;

    case FindNodeFileType:
        switch (Node->U.FileType) {
        case 'b':
            if (S_ISBLK(Stat->st_mode)) {
                *Match = TRUE;
            }

            break;

        case 'c':
            if (S_ISCHR(Stat->st_mode)) {
                *Match = TRUE;
            }

            break;

        case 'd':
            if (S_ISDIR(Stat->st_mode)) {
                *Match = TRUE;
            }

            break;

        case 'l':
            if (S_ISLNK(Stat->st_mode)) {
                *Match = TRUE;
            }

            break;

        case 'p':
            if (S_ISFIFO(Stat->st_mode)) {
                *Match = TRUE;
            }

            break;

        case 'f':
            if (S_ISREG(Stat->st_mode)) {
                *Match = TRUE;
            }

            break;

        case 's':
            if (S_ISSOCK(Stat->st_mode)) {
                *Match = TRUE;
            }

            break;

        default:

            //
            // This should have been caught during the parse phase.
            //

            assert(FALSE);

            break;
        }

        break;

    case FindNodeLinks:
        *Match = FindEvaluateIntegerTest(&(Node->U.Integer), Stat->st_nlink);
        break;

    case FindNodeUserName:
    case FindNodeUserId:
        *Match = FindEvaluateIntegerTest(&(Node->U.Integer), Stat->st_uid);
        break;

    case FindNodeGroupName:
    case FindNodeGroupId:
        *Match = FindEvaluateIntegerTest(&(Node->U.Integer), Stat->st_gid);
        break;

    case FindNodeSize:
        if (Node->U.Size.Bytes != FALSE) {
            Size = Stat->st_size;

        } else {
            Size = (Stat->st_size + (FIND_FILE_BLOCK_SIZE - 1)) /
                   FIND_FILE_BLOCK_SIZE;
        }

        *Match = FindEvaluateIntegerTest(&(Node->U.Size.Integer), Size);
        break;

    case FindNodeAccessTime:
        Time = (Stat->st_atime - Context->CurrentTime) / SECONDS_PER_DAY;
        *Match = FindEvaluateIntegerTest(&(Node->U.Integer), Time);
        break;

    case FindNodeStatusChangeTime:
        Time = (Stat->st_ctime - Context->CurrentTime) / SECONDS_PER_DAY;
        *Match = FindEvaluateIntegerTest(&(Node->U.Integer), Time);
        break;

    case FindNodeModificationTime:
        Time = (Stat->st_mtime - Context->CurrentTime) / SECONDS_PER_DAY;
        *Match = FindEvaluateIntegerTest(&(Node->U.Integer), Time);
        break;

    case FindNodeExecute:
        Result = FindEvaluateExecute(Context, Path, Node, Match);
        break;

    case FindNodeNewer:
        if (Stat->st_mtime > Node->U.ModificationTime) {
            *Match = TRUE;
        }

        break;

    default:

        assert(FALSE);

        Result = EINVAL;
        break;
    }

EvaluateNodeEnd:
    if (Node->Negate != FALSE) {
        if (*Match != FALSE) {
            *Match = FALSE;

        } else {
            *Match = TRUE;
        }
    }

    return Result;
}

INT
FindEvaluateExecute (
    PFIND_CONTEXT Context,
    PSTR Path,
    PFIND_NODE Node,
    PBOOL Match
    )

/*++

Routine Description:

    This routine evaluates an execute command for a file.

Arguments:

    Context - Supplies the application context.

    Path - Supplies a pointer to the file name. This must not be modified.

    Node - Supplies a pointer to the node to evaluate.

    Match - Supplies a pointer where a boolean will be returned indicating
        whether the given file passed the test.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    BOOL Answer;
    ULONG ArgumentCount;
    ULONG ArgumentIndex;
    PFIND_NODE_EXECUTE Execute;
    INT Result;
    INT ReturnValue;

    //
    // For batched commands, add the path to the new arguments, and return
    // immediately if the batch size isn't big enough.
    //

    assert(Node->Type == FindNodeExecute);

    ArgumentCount = 0;
    Execute = &(Node->U.Execute);
    if (Execute->Batch != FALSE) {
        *Match = TRUE;
        ArgumentIndex = Execute->ArgumentCount + Execute->BatchCount;

        assert(Execute->BatchCount < FIND_BATCH_SIZE);
        assert(Execute->NewArguments[ArgumentIndex] == NULL);

        Execute->NewArguments[ArgumentIndex] = strdup(Path);
        if (Execute->NewArguments[ArgumentIndex] == NULL) {
            Result = ENOMEM;
            goto EvaluateExecuteEnd;
        }

        Execute->BatchCount += 1;
        if (Execute->BatchCount < FIND_BATCH_SIZE) {
            Result = 0;
            goto EvaluateExecuteEnd;
        }

        ArgumentCount = Execute->ArgumentCount + Execute->BatchCount;
    }

    //
    // Prompt if needed.
    //

    if (Execute->Confirm != FALSE) {
        fprintf(stderr, "< %s ... %s > ? ", Execute->Arguments[0], Path);
        Result = SwGetYesNoAnswer(&Answer);
        if (Result != 0) {
            goto EvaluateExecuteEnd;
        }

        if (Answer == FALSE) {
            Result = 0;
            goto EvaluateExecuteEnd;
        }
    }

    //
    // Perform substitutions if needed.
    //

    if (Execute->Batch == FALSE) {
        for (ArgumentIndex = 0;
             ArgumentIndex < Execute->ArgumentCount;
             ArgumentIndex += 1) {

            assert(Execute->NewArguments[ArgumentIndex] == NULL);

            Execute->NewArguments[ArgumentIndex] =
                          FindSubstitutePath(Execute->Arguments[ArgumentIndex],
                                             Path);

            if (Execute->NewArguments[ArgumentIndex] == NULL) {
                Result = ENOMEM;
                goto EvaluateExecuteEnd;
            }
        }

        ArgumentCount = Execute->ArgumentCount;
    }

    Result = FindExecute(Execute->NewArguments, ArgumentCount, &ReturnValue);
    if (Result != 0) {
        goto EvaluateExecuteEnd;
    }

    if (ReturnValue == 0) {
        *Match = TRUE;
    }

    //
    // Wipe out the new arguments. For batched arguments, it's just the
    // arguments at the end. For non-batched ones, it's all the supplied
    // arguments.
    //

    if (Execute->Batch != FALSE) {
        for (ArgumentIndex = Execute->ArgumentCount;
             ArgumentIndex < Execute->ArgumentCount + FIND_BATCH_SIZE;
             ArgumentIndex += 1) {

            if (Execute->NewArguments[ArgumentIndex] != NULL) {
                free(Execute->NewArguments[ArgumentIndex]);
                Execute->NewArguments[ArgumentIndex] = NULL;
            }
        }

        Execute->BatchCount = 0;

    } else {
        for (ArgumentIndex = 0;
             ArgumentIndex < Execute->ArgumentCount;
             ArgumentIndex += 1) {

            if (Execute->NewArguments[ArgumentIndex] != NULL) {
                free(Execute->NewArguments[ArgumentIndex]);
                Execute->NewArguments[ArgumentIndex] = NULL;
            }
        }
    }

EvaluateExecuteEnd:
    return Result;
}

INT
FindExecute (
    PSTR *Arguments,
    INT ArgumentCount,
    PINT ReturnValue
    )

/*++

Routine Description:

    This routine executes a command for the find utility.

Arguments:

    Arguments - Supplies an array of pointers to the utility and arguments to
        execute.

    ArgumentCount - Supplies the number of arguments in the array.

    ReturnValue - Supplies a pointer where the return value from the utility
        will be returned.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    INT Result;

    Result = SwRunCommand(Arguments[0],
                          Arguments,
                          ArgumentCount,
                          FALSE,
                          ReturnValue);

    if (Result != 0) {
        SwPrintError(Result, Arguments[0], "Failed to execute");
    }

    return 0;
}

PSTR
FindSubstitutePath (
    PSTR Argument,
    PSTR Path
    )

/*++

Routine Description:

    This routine finds instances of {} in the given argument string and
    replaces them with the given path.

Arguments:

    Argument - Supplies a pointer to the argument string to use as the format.

    Path - Supplies a pointer to the replacement path string.

Return Value:

    Returns a pointer to a newly allocated argument string with any
    substitutions on success.

    NULL on allocation failure.

--*/

{

    UINTN Capacity;
    ULONG Index;
    PSTR NewArgument;
    ULONG PathSize;
    UINTN Size;

    NewArgument = strdup(Argument);
    if (NewArgument == NULL) {
        return NULL;
    }

    Size = strlen(NewArgument) + 1;
    Capacity = Size;
    PathSize = strlen(Path) + 1;
    Index = 0;
    while (NewArgument[Index] != '\0') {
        if ((NewArgument[Index] == '{') && (NewArgument[Index + 1] == '}')) {
            SwStringReplaceRegion(&NewArgument,
                                  &Size,
                                  &Capacity,
                                  Index,
                                  Index + 2,
                                  Path,
                                  PathSize);

            Index += PathSize;

        } else {
            Index += 1;
        }
    }

    return NewArgument;
}

BOOL
FindEvaluateIntegerTest (
    PFIND_NODE_INTEGER Integer,
    LONGLONG Value
    )

/*++

Routine Description:

    This routine performs an integer test comparison.

Arguments:

    Integer - Supplies a pointer to the integer value.

    Value - Supplies the value to perform the test on.

Return Value:

    TRUE if the value satisfies the integer.

    FALSE if the value does not satisfy the integer.

--*/

{

    switch (Integer->Type) {
    case FindIntegerLessThan:
        if (Value <= Integer->Value) {
            return TRUE;
        }

        break;

    case FindIntegerEqualTo:
        if (Value == Integer->Value) {
            return TRUE;
        }

        break;

    case FindIntegerGreaterThan:
        if (Value >= Integer->Value) {
            return TRUE;
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    return FALSE;
}

INT
FindFlushBatchExecutes (
    PFIND_CONTEXT Context,
    PFIND_NODE Node
    )

/*++

Routine Description:

    This routine flushes any incompletely batched execute commands.

Arguments:

    Context - Supplies the application context.

    Node - Supplies a pointer to the node to evaluate.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PFIND_NODE Child;
    PLIST_ENTRY CurrentEntry;
    PFIND_NODE_EXECUTE Execute;
    INT Result;
    INT ReturnValue;

    Result = 0;
    switch (Node->Type) {
    case FindNodeParentheses:
        CurrentEntry = Node->U.ChildList.Next;
        while (CurrentEntry != &(Node->U.ChildList)) {
            Child = LIST_VALUE(CurrentEntry, FIND_NODE, ListEntry);
            Result = FindFlushBatchExecutes(Context, Child);
            if (Result != 0) {
                return Result;
            }

            CurrentEntry = CurrentEntry->Next;
        }

    case FindNodeExecute:
        Execute = &(Node->U.Execute);
        if ((Execute->Batch == FALSE) || (Execute->BatchCount == 0)) {
            break;
        }

        Result = FindExecute(Execute->NewArguments,
                             Execute->ArgumentCount + Execute->BatchCount,
                             &ReturnValue);

        break;

    default:
        break;
    }

    return Result;
}

INT
FindParseArguments (
    PFIND_CONTEXT Context,
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine parses the arguments of a find utility invocation.

Arguments:

    Context - Supplies the application context.

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    INT ArgumentIndex;
    BOOL CanBeLast;
    PFIND_NODE NewNode;
    BOOL Not;
    PFIND_NODE Parent;
    BOOL PrintNeeded;
    INT Result;

    //
    // Get past and take note of any input paths. Also deal with the -H and -L
    // options now.
    //

    ArgumentIndex = 1;
    Context->InputIndex = ArgumentIndex;
    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];

        //
        // Deal with -H and -L, but only before the first input comes in.
        //

        if ((Context->InputIndex == ArgumentIndex) &&
            (strcmp(Argument, "-H") == 0)) {

            Context->Options |= FIND_OPTION_LINKS_IN_OPERANDS;
            Context->Options &= ~FIND_OPTION_FOLLOW_ALL_LINKS;
            Context->InputIndex = ArgumentIndex + 1;

        } else if ((Context->InputIndex == ArgumentIndex) &&
                   (strcmp(Argument, "-L") == 0)) {

            Context->Options |= FIND_OPTION_FOLLOW_ALL_LINKS;
            Context->Options &= ~FIND_OPTION_LINKS_IN_OPERANDS;

        //
        // Stop if this is the beginning of the primary expression.
        //

        } else if ((*Argument == '(') || (*Argument == '!') ||
                   (*Argument == '-')) {

            break;
        }

        ArgumentIndex += 1;
    }

    Context->InputCount = ArgumentIndex - Context->InputIndex;

    //
    // Initialize the head node.
    //

    Parent = &(Context->HeadNode);
    Parent->Type = FindNodeParentheses;
    INITIALIZE_LIST_HEAD(&(Parent->U.ChildList));

    //
    // Now parse any expression nodes.
    //

    CanBeLast = TRUE;
    PrintNeeded = TRUE;
    Not = FALSE;

    //
    // If there is nothing, everything matches.
    //

    if (ArgumentIndex == ArgumentCount) {
        FindCreateNode(FindNodeTrue, Parent);
    }

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        NewNode = NULL;
        CanBeLast = FALSE;
        if (strcmp(Argument, "!") == 0) {
            Not = TRUE;

        } else if (strcmp(Argument, "(") == 0) {
            NewNode = FindCreateNode(FindNodeParentheses, Parent);
            if (NewNode == NULL) {
                Result = ENOMEM;
                goto FindParseArgumentsEnd;
            }

            Parent = NewNode;

        } else if (strcmp(Argument, ")") == 0) {
            CanBeLast = TRUE;
            if (Parent->Parent == NULL) {
                SwPrintError(0, NULL, "Unbalanced )");
                Result = EINVAL;
                goto FindParseArgumentsEnd;
            }

            if (Parent->Type != FindNodeParentheses) {
                SwPrintError(0, NULL, "Unexpected )");
                Result = EINVAL;
                goto FindParseArgumentsEnd;
            }

            Parent = Parent->Parent;
            Not = FALSE;

        } else if (strcmp(Argument, "-a") == 0) {

            //
            // ANDs are always implied. The only difference with an explicit
            // one is if there is no left side, then add a print.
            //

            if (LIST_EMPTY(&(Parent->U.ChildList)) != FALSE) {
                SwPrintError(0, NULL, "-a used with nothing before it");
                Result = EINVAL;
                goto FindParseArgumentsEnd;
            }

            Not = FALSE;

        } else {
            if (strcmp(Argument, "-o") == 0) {
                if (LIST_EMPTY(&(Parent->U.ChildList)) != FALSE) {
                    SwPrintError(0, NULL, "-o used with nothing before it");
                    Result = EINVAL;
                    goto FindParseArgumentsEnd;
                }

            } else {
                CanBeLast = TRUE;
            }

            Result = FindParseNode(Context,
                                   Parent,
                                   Arguments,
                                   ArgumentCount,
                                   &ArgumentIndex,
                                   &NewNode);

            if (Result != 0) {
                goto FindParseArgumentsEnd;
            }

            //
            // If the expression contains an exec, ok, or print anywhere, then
            // there's no need to tack on a default print.
            //

            if ((NewNode->Type == FindNodeExecute) ||
                (NewNode->Type == FindNodePrint)) {

                PrintNeeded = FALSE;
            }

            //
            // Subtract one to account for the upcoming addition.
            //

            ArgumentIndex -= 1;
        }

        if ((NewNode != FALSE) && (Not != FALSE)) {
            if (NewNode->Type == FindNodeOr) {
                SwPrintError(0, NULL, "unexpected ! before -o");
                FindDestroyNode(NewNode);
                goto FindParseArgumentsEnd;
            }

            NewNode->Negate = Not;
            Not = FALSE;
        }

        ArgumentIndex += 1;
    }

    if (CanBeLast == FALSE) {
        SwPrintError(0, Arguments[ArgumentCount - 1], "Invalid last argument");
        Result = EINVAL;
        goto FindParseArgumentsEnd;
    }

    //
    // If a print is needed, add it.
    //

    if (PrintNeeded != FALSE) {
        Context->Options |= FIND_OPTION_IMPLIED_PRINT;
    }

    Result = 0;

FindParseArgumentsEnd:
    return Result;
}

INT
FindParseNode (
    PFIND_CONTEXT Context,
    PFIND_NODE Parent,
    PSTR *Arguments,
    INT ArgumentCount,
    PINT ArgumentIndex,
    PFIND_NODE *CreatedNode
    )

/*++

Routine Description:

    This routine parses a new non-grammatical node.

Arguments:

    Context - Supplies a pointer to the application context.

    Parent - Supplies the parent node of this soon-to-be-created node.

    Arguments - Supplies the complete command line arguments.

    ArgumentCount - Supplies the number of arguments in the array.

    ArgumentIndex - Supplies a pointer that on input contains the index of the
        argument containing the node to parse. On output this index will be
        updated beyond the parsed node.

    CreatedNode - Supplies a pointer where a newly created node will be
        returned on success.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR AfterScan;
    PSTR Argument;
    size_t ArgumentLength;
    INT BeginIndex;
    PFIND_NODE_EXECUTE Execute;
    BOOL ExecuteConfirmation;
    BOOL FollowLinks;
    INT Index;
    ULONG NewArgumentIndex;
    PFIND_NODE NewNode;
    INT NextIndex;
    INT Result;
    BOOL SingleArgument;
    struct stat Stat;
    size_t TotalArgumentCount;
    FIND_NODE_TYPE Type;
    id_t UserOrGroupId;
    BOOL WasBraces;

    Index = *ArgumentIndex;
    Argument = Arguments[Index];
    NewNode = NULL;
    ExecuteConfirmation = FALSE;
    SingleArgument = FALSE;

    //
    // Fail if this doesn't start with a dash.
    //

    if (*Argument != '-') {
        SwPrintError(0, Argument, "Paths must precede predicates");
        Result = EINVAL;
        goto ParseNodeEnd;
    }

    //
    // Establish the type. Assume all predicates take a single argument, and
    // fix it up for those that don't.
    //

    SingleArgument = TRUE;
    Argument += 1;
    if (strcmp(Argument, "name") == 0) {
        Type = FindNodeName;

    } else if (strcmp(Argument, "nouser") == 0) {
        Type = FindNodeNoUser;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "nogroup") == 0) {
        Type = FindNodeNoGroup;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "xdev") == 0) {
        Context->Options |= FIND_OPTION_NO_CROSS_DEVICE;
        Type = FindNodeTrue;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "prune") == 0) {
        Type = FindNodePrune;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "perm") == 0) {
        Type = FindNodePermissions;

    } else if (strcmp(Argument, "type") == 0) {
        Type = FindNodeFileType;

    } else if (strcmp(Argument, "links") == 0) {
        Type = FindNodeLinks;

    } else if (strcmp(Argument, "user") == 0) {
        Type = FindNodeUserName;

    } else if (strcmp(Argument, "group") == 0) {
        Type = FindNodeGroupName;

    } else if (strcmp(Argument, "uid") == 0) {
        Type = FindNodeUserId;

    } else if (strcmp(Argument, "gid") == 0) {
        Type = FindNodeGroupId;

    } else if (strcmp(Argument, "size") == 0) {
        Type = FindNodeSize;

    } else if (strcmp(Argument, "atime") == 0) {
        Type = FindNodeAccessTime;

    } else if (strcmp(Argument, "ctime") == 0) {
        Type = FindNodeStatusChangeTime;

    } else if (strcmp(Argument, "mtime") == 0) {
        Type = FindNodeModificationTime;

    } else if (strcmp(Argument, "exec") == 0) {
        Type = FindNodeExecute;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "ok") == 0) {
        Type = FindNodeExecute;
        ExecuteConfirmation = TRUE;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "o") == 0) {
        Type = FindNodeOr;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "print") == 0) {
        Type = FindNodePrint;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "newer") == 0) {
        Type = FindNodeNewer;

    } else if (strcmp(Argument, "depth") == 0) {
        Context->Options |= FIND_OPTION_DEPTH_FIRST;
        Type = FindNodeTrue;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "true") == 0) {
        Type = FindNodeTrue;
        SingleArgument = FALSE;

    } else if (strcmp(Argument, "false") == 0) {
        Type = FindNodeFalse;
        SingleArgument = FALSE;

    } else {
        SwPrintError(0, Argument, "Unrecognized predicate");
        Result = EINVAL;
        goto ParseNodeEnd;
    }

    //
    // If there's a required argument and it's not there, fail.
    //

    if (((SingleArgument != FALSE) || (Type == FindNodeExecute)) &&
        (Index + 1 >= ArgumentCount)) {

        SwPrintError(0,
                     NULL,
                     "%s takes an argument. Try --help for usage",
                     Argument);

        Result = EINVAL;
        goto ParseNodeEnd;
    }

    Index += 1;
    if (SingleArgument != FALSE) {
        Argument = Arguments[Index];
        Index += 1;
    }

    //
    // Create the node.
    //

    NewNode = FindCreateNode(Type, Parent);
    if (NewNode == NULL) {
        Result = ENOMEM;
        goto ParseNodeEnd;
    }

    switch (Type) {
    case FindNodeName:
        NewNode->U.Name.Pattern = Argument;
        NewNode->U.Name.PatternSize = strlen(Argument) + 1;
        break;

    case FindNodePermissions:
        if (Argument[0] == '-') {
            Argument += 1;

        } else {
            NewNode->U.Permissions.Exact = TRUE;
        }

        Result = SwParseFilePermissionsString(
                                        Argument,
                                        FALSE,
                                        &(NewNode->U.Permissions.Permissions));

        if (Result == FALSE) {
            SwPrintError(0, Argument, "Failed to parse permissions");
            Result = EINVAL;
            goto ParseNodeEnd;
        }

        break;

    case FindNodeFileType:
        if (strlen(Argument) != 1) {
            SwPrintError(0, Argument, "Invalid file type");
            Result = EINVAL;
            goto ParseNodeEnd;
        }

        if ((*Argument != 'b') && (*Argument != 'c') && (*Argument != 'd') &&
            (*Argument != 'l') && (*Argument != 'p') && (*Argument != 'f') &&
            (*Argument != 's')) {

            SwPrintError(0, Argument, "Unrecognized file type");
            Result = EINVAL;
            goto ParseNodeEnd;
        }

        NewNode->U.FileType = *Argument;
        break;

    case FindNodeLinks:
    case FindNodeAccessTime:
    case FindNodeStatusChangeTime:
    case FindNodeModificationTime:
    case FindNodeUserId:
    case FindNodeGroupId:
        Result = FindParseInteger(Argument, &(NewNode->U.Integer));
        if (Result != 0) {
            SwPrintError(0, Argument, "Failed to parse integer");
            goto ParseNodeEnd;
        }

        break;

    case FindNodeUserName:
        Result = SwGetUserIdFromName(Argument, (uid_t *)&UserOrGroupId);
        NewNode->U.Integer.Type = FindIntegerEqualTo;
        if (Result == 0) {
            NewNode->U.Integer.Value = UserOrGroupId;

        } else {
            NewNode->U.Integer.Value = strtoul(Argument, &AfterScan, 10);
            if (AfterScan == Argument) {
                SwPrintError(0, Argument, "Invalid user name or ID");
                goto ParseNodeEnd;
            }
        }

        break;

    case FindNodeGroupName:
        Result = SwGetGroupIdFromName(Argument, (gid_t *)&UserOrGroupId);
        NewNode->U.Integer.Type = FindIntegerEqualTo;
        if (Result == 0) {
            NewNode->U.Integer.Value = UserOrGroupId;

        } else {
            NewNode->U.Integer.Value = strtoul(Argument, &AfterScan, 10);
            if (AfterScan == Argument) {
                SwPrintError(0, Argument, "Invalid group name or ID");
                goto ParseNodeEnd;
            }
        }

        break;

    case FindNodeSize:
        Result = FindParseInteger(Argument, &(NewNode->U.Size.Integer));
        if (Result != 0) {
            SwPrintError(0, Argument, "Failed to parse integer");
            goto ParseNodeEnd;
        }

        ArgumentLength = strlen(Argument);
        if ((ArgumentLength != 0) && (Argument[ArgumentLength - 1] == 'c')) {
            NewNode->U.Size.Bytes = TRUE;
        }

        break;

    case FindNodeExecute:
        Execute = &(NewNode->U.Execute);
        Execute->Confirm = ExecuteConfirmation;
        BeginIndex = Index;
        WasBraces = FALSE;
        NextIndex = Index;
        while (Index < ArgumentCount) {
            Argument = Arguments[Index];
            if (strcmp(Argument, ";") == 0) {
                NextIndex = Index + 1;
                break;

            //
            // The batch mode stuff can only happen on -exec, not -ok.
            //

            } else if (ExecuteConfirmation == FALSE) {
                if (strcmp(Argument, "{}") == 0) {
                    WasBraces = TRUE;

                } else if ((WasBraces != FALSE) &&
                           (strcmp(Argument, "+") == 0)) {

                    NextIndex = Index + 1;

                    //
                    // The {} are not included.
                    //

                    Index -= 1;
                    Execute->Batch = TRUE;
                    break;

                } else {
                    WasBraces = FALSE;
                }
            }

            Index += 1;
        }

        if ((Index == BeginIndex) || (Index == ArgumentCount)) {
            SwPrintError(0, NULL, "Missing argument to -exec");
            Result = EINVAL;
            goto ParseNodeEnd;
        }

        Execute->Arguments = &(Arguments[BeginIndex]);
        Execute->ArgumentCount = Index - BeginIndex;
        TotalArgumentCount = Execute->ArgumentCount + 1;
        if (Execute->Batch != FALSE) {
            TotalArgumentCount += FIND_BATCH_SIZE;
        }

        Execute->NewArguments = malloc(TotalArgumentCount * sizeof(PSTR));
        if (Execute->NewArguments == NULL) {
            Result = ENOMEM;
            goto ParseNodeEnd;
        }

        memset(Execute->NewArguments, 0, TotalArgumentCount * sizeof(PSTR));

        //
        // For batched execute nodes, copy the supplied arguments over verbatim.
        //

        if (Execute->Batch != FALSE) {
            for (NewArgumentIndex = 0;
                 NewArgumentIndex < Execute->ArgumentCount;
                 NewArgumentIndex += 1) {

                Execute->NewArguments[NewArgumentIndex] =
                                  strdup(Execute->Arguments[NewArgumentIndex]);

                if (Execute->NewArguments[NewArgumentIndex] == NULL) {
                    Result = ENOMEM;
                    goto ParseNodeEnd;
                }
            }
        }

        Index = NextIndex;
        break;

    case FindNodeNewer:
        FollowLinks = FALSE;
        if ((Context->Options & FIND_OPTION_LINK_MASK) != 0) {
            FollowLinks = TRUE;
        }

        Result = SwStat(Argument, FollowLinks, &Stat);
        if (Result != 0) {
            SwPrintError(Result, Argument, "Failed to stat -newer argument");
            goto ParseNodeEnd;
        }

        NewNode->U.ModificationTime = Stat.st_mtime;
        break;

    //
    // Do nothing for types that take no argument.
    //

    case FindNodeOr:
    case FindNodePrint:
    case FindNodeNoUser:
    case FindNodeNoGroup:
    case FindNodeTrue:
    case FindNodePrune:
        break;

    default:

        assert(FALSE);

        Result = EINVAL;
        goto ParseNodeEnd;
    }

    Result = 0;

ParseNodeEnd:
    if (Result != 0) {
        if (NewNode != NULL) {
            FindDestroyNode(NewNode);
            NewNode = NULL;
        }
    }

    *ArgumentIndex = Index;
    *CreatedNode = NewNode;
    return Result;
}

INT
FindParseInteger (
    PSTR Argument,
    PFIND_NODE_INTEGER Integer
    )

/*++

Routine Description:

    This routine parses an integer operand, which is simply a decimal integer
    prefixed with an optional plus or minus.

Arguments:

    Argument - Supplies a pointer to the string containing the integer.

    Integer - Supplies a pointer where the integer value will be returned.

Return Value:

    0 on success.

    EINVAL on failure.

--*/

{

    PSTR AfterScan;

    Integer->Type = FindIntegerEqualTo;
    if (*Argument == '+') {
        Integer->Type = FindIntegerGreaterThan;
        Argument += 1;

    } else if (*Argument == '-') {
        Integer->Type = FindIntegerLessThan;
        Argument += 1;
    }

    Integer->Value = strtol(Argument, &AfterScan, 10);
    if (AfterScan == Argument) {
        return EINVAL;
    }

    return 0;
}

PFIND_NODE
FindCreateNode (
    FIND_NODE_TYPE Type,
    PFIND_NODE Parent
    )

/*++

Routine Description:

    This routine creates a find node.

Arguments:

    Type - Supplies the type of node to create.

    Parent - Supplies a pointer to set for the node's parent. The node will
        be added to the child list of this parent.

Return Value:

    Returns a pointer to the node on success.

    NULL on failure.

--*/

{

    PFIND_NODE NewNode;

    NewNode = malloc(sizeof(FIND_NODE));
    if (NewNode == NULL) {
        return NULL;
    }

    memset(NewNode, 0, sizeof(FIND_NODE));
    NewNode->Type = Type;
    NewNode->Parent = Parent;

    assert(Parent->Type == FindNodeParentheses);

    INSERT_BEFORE(&(NewNode->ListEntry), &(Parent->U.ChildList));
    switch (Type) {
    case FindNodeParentheses:
        INITIALIZE_LIST_HEAD(&(NewNode->U.ChildList));
        break;

    case FindNodeOr:
    case FindNodePrint:
    case FindNodeName:
    case FindNodeNoUser:
    case FindNodeNoGroup:
    case FindNodeTrue:
    case FindNodeFalse:
    case FindNodePrune:
    case FindNodePermissions:
    case FindNodeFileType:
    case FindNodeLinks:
    case FindNodeUserName:
    case FindNodeGroupName:
    case FindNodeUserId:
    case FindNodeGroupId:
    case FindNodeSize:
    case FindNodeAccessTime:
    case FindNodeStatusChangeTime:
    case FindNodeModificationTime:
    case FindNodeExecute:
    case FindNodeNewer:
        break;

    default:

        assert(FALSE);

        free(NewNode);
        NewNode = NULL;
        break;
    }

    return NewNode;
}

VOID
FindDestroyNode (
    PFIND_NODE Node
    )

/*++

Routine Description:

    This routine destroys a find node.

Arguments:

    Node - Supplies a pointer to the node to destroy.

Return Value:

    None.

--*/

{

    ULONG ArgumentCount;
    PFIND_NODE Child;
    PFIND_NODE_EXECUTE Execute;
    ULONG Index;

    LIST_REMOVE(&(Node->ListEntry));
    switch (Node->Type) {
    case FindNodeParentheses:
        while (LIST_EMPTY(&(Node->U.ChildList)) == FALSE) {
            Child = LIST_VALUE(Node->U.ChildList.Next, FIND_NODE, ListEntry);
            FindDestroyNode(Child);
        }

        break;

    case FindNodeExecute:
        Execute = &(Node->U.Execute);
        ArgumentCount = Execute->ArgumentCount;
        if (Execute->Batch != FALSE) {
            ArgumentCount += FIND_BATCH_SIZE;
        }

        if (Execute->NewArguments != NULL) {
            for (Index = 0; Index < ArgumentCount; Index += 1) {
                if (Execute->NewArguments[Index] != NULL) {
                    free(Execute->NewArguments[Index]);
                }
            }

            free(Execute->NewArguments);
        }

        break;

    case FindNodeOr:
    case FindNodePrint:
    case FindNodeName:
    case FindNodeNoUser:
    case FindNodeNoGroup:
    case FindNodeTrue:
    case FindNodeFalse:
    case FindNodePrune:
    case FindNodePermissions:
    case FindNodeFileType:
    case FindNodeLinks:
    case FindNodeUserName:
    case FindNodeGroupName:
    case FindNodeUserId:
    case FindNodeGroupId:
    case FindNodeSize:
    case FindNodeAccessTime:
    case FindNodeStatusChangeTime:
    case FindNodeModificationTime:
    case FindNodeNewer:
        break;

    default:

        assert(FALSE);

        break;
    }

    free(Node);
    return;
}

INT
FindAddSearchedDirectory (
    PFIND_CONTEXT Context,
    dev_t Device,
    ino_t FileNumber,
    PBOOL AlreadyVisited
    )

/*++

Routine Description:

    This routine adds a directory to the array of searched directories.

Arguments:

    Context - Supplies a pointer to the application context.

    Device - Supplies the device number of the directory that was just visited.

    FileNumber - Supplies the file serial number of the directory that was
        just visited.

    AlreadyVisited - Supplies a pointer where a boolean will be returned
        indicating if the directory was already in the array.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PFIND_VISIT Element;
    ULONG Index;
    PVOID NewBuffer;
    ULONG NewCapacity;

    //
    // First search the array. Don't match on file number of zero, as it's
    // very probably an indication that the OS doesn't support file numbers.
    // If there really was a loop in file number 0, well then sorry.
    //

    for (Index = 0; Index < Context->SearchedDirectoryCount; Index += 1) {
        Element = &(Context->SearchedDirectories[Index]);
        if ((Element->FileNumber == FileNumber) &&
            (FileNumber != 0) &&
            (Element->Device == Device)) {

            *AlreadyVisited = TRUE;
            return 0;
        }
    }

    //
    // The element wasn't found in there. Expand the array if it needs it.
    //

    *AlreadyVisited = FALSE;
    if (Index == Context->SearchedDirectoryCapacity) {
        NewCapacity = Context->SearchedDirectoryCapacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = FIND_SEARCHED_DIRECTORY_INITIAL_SIZE;
        }

        assert(Index + 1 < NewCapacity);

        NewBuffer = realloc(Context->SearchedDirectories,
                            NewCapacity * sizeof(FIND_VISIT));

        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        Context->SearchedDirectories = NewBuffer;
        Context->SearchedDirectoryCapacity = NewCapacity;
    }

    Context->SearchedDirectories[Index].Device = Device;
    Context->SearchedDirectories[Index].FileNumber = FileNumber;
    Context->SearchedDirectoryCount += 1;
    return 0;
}


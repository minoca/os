/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    mingen.h

Abstract:

    This header contains definitions for the Minoca Build Generator.

Author:

    Evan Green 3-Dec-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include "chalk.h"

//
// --------------------------------------------------------------------- Macros
//

#define MINGEN_IS_SPECIAL_PATH_CHARACTER(_Character) \
    (((_Character) == '/') || ((_Character) == '\\') || ((_Character) == '^'))

#define MINGEN_IS_SOURCE_ROOT_RELATIVE(_String) \
    (((_String)[0] == '/') && ((_String)[1] == '/'))

#define MINGEN_IS_BUILD_ROOT_RELATIVE(_String) \
    (((_String)[0] == '^') && ((_String)[1] == '/'))

#define MINGEN_IS_ABSOLUTE_PATH(_String) \
    (((_String)[0] == '/') || ((_String)[0] == '\\') || \
     ((isalpha((_String)[0]) != 0) && ((_String)[1] == ':') && \
      (((_String)[2] == '/') || ((_String)[2] == '\\'))))

#define MINGEN_IS_NAME(_Character) \
    ((MINGEN_IS_NAME0(_Character)) || \
     (((_Character) >= '0') && ((_Character) <= '9')))

#define MINGEN_IS_NAME0(_Character) \
    ((((_Character) >= 'A') && ((_Character) <= 'Z')) || \
     (((_Character) >= 'a') && ((_Character) <= 'z')) || \
     ((_Character) == '_'))

//
// ---------------------------------------------------------------- Definitions
//

#define MINGEN_PROJECT_FILE ".mgproj"
#define MINGEN_BUILD_FILE "build.ck"
#define MINGEN_DEFAULT_NAME "//:"

#define MINGEN_BUILD_DIRECTORIES_FILE ".builddirs"
#define MINGEN_VARIABLE_SOURCE_ROOT "SOURCE_ROOT"
#define MINGEN_VARIABLE_BUILD_ROOT "BUILD_ROOT"
#define MINGEN_VARIABLE_PROJECT_PATH "MG_PROJECT_PATH"

#define MINGEN_OPTION_VERBOSE 0x00000001
#define MINGEN_OPTION_DEBUG 0x00000002
#define MINGEN_OPTION_DRY_RUN 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MINGEN_DIRECTORY_TREE {
    MingenDirectoryTreeInvalid,
    MingenSourceTree,
    MingenBuildTree,
    MingenAbsolutePath,
} MINGEN_DIRECTORY_TREE, *PMINGEN_DIRECTORY_TREE;

typedef enum _MINGEN_INPUT_TYPE {
    MingenInputInvalid,
    MingenInputSource,
    MingenInputTarget,
} MINGEN_INPUT_TYPE, *PMINGEN_INPUT_TYPE;

typedef enum _MINGEN_SCRIPT_ORDER {
    MingenScriptOrderInvalid,
    MingenScriptOrderCommandLine,
    MingenScriptOrderProjectRoot,
    MingenScriptOrderGlobal,
    MingenScriptOrderTarget,
} MINGEN_SCRIPT_ORDER, *PMINGEN_SCRIPT_ORDER;

typedef enum _MINGEN_OUTPUT_FORMAT {
    MingenOutputInvalid,
    MingenOutputNone,
    MingenOutputMake,
    MingenOutputNinja
} MINGEN_OUTPUT_FORMAT, *PMINGEN_OUTPUT_FORMAT;

typedef struct _MINGEN_TARGET MINGEN_TARGET, *PMINGEN_TARGET;

/*++

Structure Description:

    This structure stores the components of a fully specified build target path.

Members:

    Root - Stores the directory tree root for the target.

    Path - Stores the directory path relative to the root of the target.

    Target - Stores the target name.

--*/

typedef struct _MINGEN_PATH {
    MINGEN_DIRECTORY_TREE Root;
    PSTR Path;
    PSTR Target;
} MINGEN_PATH, *PMINGEN_PATH;

/*++

Structure Description:

    This structure stores an array of paths.

Members:

    Array - Stores a pointer to the array of pointers to elements.

    Count - Stores the number of elements in the array.

    Capacity - Stores the maximum number of elements in the array before the
        array must be resized.

--*/

typedef struct _MINGEN_PATH_LIST {
    PMINGEN_PATH Array;
    ULONG Count;
    ULONG Capacity;
} MINGEN_PATH_LIST, *PMINGEN_PATH_LIST;

/*++

Structure Description:

    This structure stores the global information for an instantiation of the
    Minoca Build Generator application.

Members:

    Executable - Stores the value of argv[0].

    Options - Stores the bitfield of application options. see MINGEN_OPTION_*
        definitions.

    Interpreter - Stores the Chalk interpreter context.

    Format - Stores the final output build file format.

    FormatString - Stores the default format string specified in the project
        root.

    ProjectFilePath - Stores a pointer to the complete path to the project
        file.

    BuildFileName - Stores a pointer ot the build file name to look for in each
        directory.

    SourceRoot - Stores a pointer to the source root directory.

    BuildRoot - Stores a pointer to the build root directory.

    ScriptList - Stores the head of the list of loaded scripts.

    GlobalName - Stores a pointer to the name of the global environment file
        to load into all targets.

    DefaultName - Stores a pointer to the name of the default target. If not
        specified, defaults to //:.

    ToolList - Stores a list of tools defined. Tools are global across the
        build.

    GlobalConfig - Stores an optional pointer to the dictionary of global
        configuration variables.

    PoolList - Stores the list of pools defined.

    BuildDirectories - Stores the array of build directories.

    CommandScripts - Stores a pointer to an array of command line scripts that
        were specified.

    CommandScriptCount - Stores the number of command line scripts in the
        array.

--*/

typedef struct _MINGEN_CONTEXT {
    PSTR Executable;
    ULONG Options;
    CHALK_INTERPRETER Interpreter;
    MINGEN_OUTPUT_FORMAT Format;
    PSTR FormatString;
    PSTR ProjectFilePath;
    PSTR BuildFileName;
    PSTR SourceRoot;
    PSTR BuildRoot;
    LIST_ENTRY ScriptList;
    PSTR GlobalName;
    PSTR DefaultName;
    LIST_ENTRY ToolList;
    PCHALK_OBJECT GlobalConfig;
    LIST_ENTRY PoolList;
    MINGEN_PATH_LIST BuildDirectories;
    PSTR *CommandScripts;
    ULONG CommandScriptCount;
} MINGEN_CONTEXT, *PMINGEN_CONTEXT;

/*++

Structure Description:

    This structure stores an array of inputs, which are either of type source
    or target.

Members:

    Array - Stores a pointer to the array of pointers to elements. Each element
        is of type MINGEN_SOURCE or MINGEN_TARGET depending on their first field
        value.

    Count - Stores the number of elements in the array.

    Capacity - Stores the maximum number of elements in the array before the
        array must be resized.

--*/

typedef struct _MINGEN_INPUTS {
    PVOID *Array;
    ULONG Count;
    ULONG Capacity;
} MINGEN_INPUTS, *PMINGEN_INPUTS;

/*++

Structure Description:

    This structure stores a tool definition.

Members:

    ListEntry - Stores pointers to the next and previous tools in the build.

    Name - Stores a pointer to the name of the tool. This must be unique across
        the build.

    Command - Stores a pointer to the command to run to execute the tool.

    Description - Stores a pointer to the description to print when executing
        the tool.

    Depfile - Stores a pointer to the dependency file output name.

    DepsFormat - Stores a pointer to the dependency file format: MSVC or GCC.

    Pool - Stores an optional pointer to the pool this tool belongs in. This is
        only supported by Ninja builds.

--*/

typedef struct _MINGEN_TOOL {
    LIST_ENTRY ListEntry;
    PSTR Name;
    PSTR Command;
    PSTR Description;
    PSTR Depfile;
    PSTR DepsFormat;
    PSTR Pool;
} MINGEN_TOOL, *PMINGEN_TOOL;

/*++

Structure Description:

    This structure stores information about a loaded script.

Members:

    ListEntry - Stores pointers to the next and previous scripts in the build.

    Root - Stores the directory tree root for the target.

    Path - Stores the directory path relative to the root, not including the
        actual file name.

    CompletePath - Stores the complete file path to the script.

    Order - Stores the script order.

    Script - Stores a pointer to the script contents.

    Size - Stores the size of the script file in bytes, not including the
        artificially appended null terminator.

    Result - Stores the result returned from executing the script.

    TargetList - Stores the head of the list of targets in this script.

    TargetCount - Stores the number of targets on the target list.

--*/

typedef struct _MINGEN_SCRIPT {
    LIST_ENTRY ListEntry;
    MINGEN_DIRECTORY_TREE Root;
    PSTR Path;
    PSTR CompletePath;
    MINGEN_SCRIPT_ORDER Order;
    PSTR Script;
    UINTN Size;
    PCHALK_OBJECT Result;
    LIST_ENTRY TargetList;
    ULONG TargetCount;
} MINGEN_SCRIPT, *PMINGEN_SCRIPT;

/*++

Structure Description:

    This structure stores information about a loaded script.

Members:

    ListEntry - Stores pointers to the next and previous scripts in the build.

    Root - Stores the directory tree root for the target.

    Path - Stores the directory path relative to the root, not including the
        actual file name.

    CompletePath - Stores the complete file path to the script.

    Script - Stores a pointer to the script contents.

    Size - Stores the size of the script file in bytes, not including the
        artificially appended null terminator.

    Result - Stores the result returned from executing the script.

    TargetList - Stores the head of the list of targets in this script.

    TargetCount - Stores the number of targets on the target list.

--*/

typedef struct _MINGEN_SOURCE {
    MINGEN_INPUT_TYPE Type;
    MINGEN_DIRECTORY_TREE Tree;
    PSTR Path;
} MINGEN_SOURCE, *PMINGEN_SOURCE;

/*++

Structure Description:

    This structure stores a target definition.

Members:

    Type - Stores the type, which is always set to "target" since this is a
        target.

    ListEntry - Stores pointers to the next and previous tools in the build.

    Script - Stores a pointer back to the script that owns this target.

    Label - Stores a pointer to the name of the target.

    Output - Stores a pointer to the output name.

    Tree - Stores the tree the output path is rooted to.

    Tool - Stores the name of the tool used to build this target.

    Pool - Stores an optional name of the pool this target belongs in. This
        is only applicable to Ninja builds.

    Flags - Stores a bitfield of flags regarding the target. See MINGEN_TARGET_*
        definitions.

    Inputs - Stores the inputs to the target.

    Implicit - Stores the implicit inputs to the target. Implicit inputs work
        just like normal inputs except they don't show up in the inputs
        variable.

    OrderOnly - Stores the order-only inputs to the target. Order-only inputs
        are built before the target, but they alone do not cause the target
        to be rebuilt, and they do not show up on the input line.

    InputsObject - Stores a pointer to the list object of input strings.

    OrderOnlyObject - Stores a pointer to the list of order-only input strings.

    Callback - Stores a pointer to the function to call back when this target
        is added as an input (but not an order-only input).

    Config - Stores a pointer to the configuration information for this target.

    OriginalEntry - Stores a pointer to the original entry dictionary, which is
        passed into the callback function. The reference to this object is
        being held by the script.

--*/

struct _MINGEN_TARGET {
    MINGEN_INPUT_TYPE Type;
    LIST_ENTRY ListEntry;
    PMINGEN_SCRIPT Script;
    PSTR Label;
    PSTR Output;
    MINGEN_DIRECTORY_TREE Tree;
    PSTR Tool;
    PSTR Pool;
    ULONG Flags;
    MINGEN_INPUTS Inputs;
    MINGEN_INPUTS Implicit;
    MINGEN_INPUTS OrderOnly;
    PCHALK_OBJECT InputsObject;
    PCHALK_OBJECT ImplicitObject;
    PCHALK_OBJECT OrderOnlyObject;
    PCHALK_OBJECT Callback;
    PCHALK_OBJECT Config;
    PCHALK_OBJECT OriginalEntry;
};

/*++

Structure Description:

    This structure stores information about a build pool (only applies to
    Ninja).

Members:

    ListEntry - Stores pointers to the next and previous pools.

    Name - Stores the name of the pool.

    Depth - Stores the depth of the pool.

--*/

typedef struct _MINGEN_POOL {
    LIST_ENTRY ListEntry;
    PSTR Name;
    LONG Depth;
} MINGEN_POOL, *PMINGEN_POOL;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Main application functions
//

VOID
MingenPrintRebuildCommand (
    PMINGEN_CONTEXT Context,
    FILE *File
    );

/*++

Routine Description:

    This routine prints the command needed to re-execute this invocation of
    the program.

Arguments:

    Context - Supplies a pointer to the context.

    File - Supplies a pointer to the file to print to.

Return Value:

    None.

--*/

INT
MingenParseScriptResults (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script
    );

/*++

Routine Description:

    This routine parses the return value of a target script.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the script that just finished executing.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

VOID
MingenDestroyTarget (
    PMINGEN_TARGET Target
    );

/*++

Routine Description:

    This routine destroys a target entry.

Arguments:

    Target - Supplies a pointer to the target to destroy.

Return Value:

    None.

--*/

//
// Script utility functions
//

INT
MingenLoadTargetScript (
    PMINGEN_CONTEXT Context,
    PMINGEN_PATH Target,
    PMINGEN_SCRIPT *Script
    );

/*++

Routine Description:

    This routine loads the script corresponding to the given target specifier
    string.

Arguments:

    Context - Supplies a pointer to the application context.

    Target - Supplies a pointer to the target specifier to load.

    Script - Supplies a pointer where a pointer to the loaded or found script
        will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
MingenLoadProjectRoot (
    PMINGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine loads and interprets the project root script.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
MingenLoadScript (
    PMINGEN_CONTEXT Context,
    MINGEN_SCRIPT_ORDER Order,
    PMINGEN_PATH TargetPath,
    PMINGEN_SCRIPT *FinalScript
    );

/*++

Routine Description:

    This routine loads and interprets a given target path. If the script
    containing the given target path is already loaded, then this is a no-op.

Arguments:

    Context - Supplies a pointer to the application context.

    Order - Supplies the order to apply to the script.

    TargetPath - Supplies a pointer to the target path to load. The target
        name is ignored, only the root and path are observed.

    FinalScript - Supplies a pointer where a pointer to the newly loaded or
        found script will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

VOID
MingenDestroyAllScripts (
    PMINGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys all scripts in the application context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// Path utility functions
//

INT
MingenParsePath (
    PMINGEN_CONTEXT Context,
    PSTR Name,
    MINGEN_DIRECTORY_TREE RelativeTree,
    PSTR RelativePath,
    PMINGEN_PATH Target
    );

/*++

Routine Description:

    This routine breaks a mingen path string into its components.

Arguments:

    Context - Supplies a pointer to the application context.

    Name - Supplies a pointer to the name string.

    RelativeTree - Supplies the tree type (usually source or build) that
        relative paths are rooted against.

    RelativePath - Supplies a pointer to the path to prepend to relative paths.

    Target - Supplies a pointer where the target will be returned on success.
        The caller will be responsible for freeing the string buffers.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PSTR
MingenAppendPaths3 (
    PSTR Path1,
    PSTR Path2,
    PSTR Path3
    );

/*++

Routine Description:

    This routine appends three paths to one another.

Arguments:

    Path1 - Supplies a pointer to the first path.

    Path2 - Supplies a pointer to the second path.

    Path3 - Supplies a pointer to the second path.

Return Value:

    Returns a pointer to a newly created combined path on success. The caller
    is responsible for freeing this new path.

    NULL on allocation failure.

--*/

PSTR
MingenAppendPaths (
    PSTR Path1,
    PSTR Path2
    );

/*++

Routine Description:

    This routine appends two paths to one another.

Arguments:

    Path1 - Supplies a pointer to the first path.

    Path2 - Supplies a pointer to the second path.

Return Value:

    Returns a pointer to a newly created combined path on success. The caller
    is responsible for freeing this new path.

    NULL on allocation failure.

--*/

INT
MingenFindProjectFile (
    PMINGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine finds the top level project file.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
MingenFindSourceRoot (
    PMINGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine nails down the source root directory.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

PSTR
MingenPathForTree (
    PMINGEN_CONTEXT Context,
    MINGEN_DIRECTORY_TREE Tree
    );

/*++

Routine Description:

    This routine returns the path for the given tree root.

Arguments:

    Context - Supplies a pointer to the context.

    Tree - Supplies the root to return.

Return Value:

    Returns the path of the given tree. The caller does not own this memory.

--*/

PSTR
MingenSplitExtension (
    PSTR Path,
    PSTR *Extension
    );

/*++

Routine Description:

    This routine splits the extension portion off the end of a file path.

Arguments:

    Path - Supplies a pointer to the path to split.

    Extension - Supplies a pointer where a pointer to the extension will be
        returned on success. This memory will be part of the return value
        allocation, and does not need to be explicitly freed. This returns NULL
        if the path contains no extension or is a directory (ends in a slash).

Return Value:

    Returns a copy of the string, chopped before the last period. It is the
    caller's responsibility to free this memory.

    NULL on allocation failure.

--*/

VOID
MingenSplitPath (
    PSTR Path,
    PSTR *DirectoryName,
    PSTR *FileName
    );

/*++

Routine Description:

    This routine splits the directory and the file portion of a path.

Arguments:

    Path - Supplies a pointer to the path to split in line.

    DirectoryName - Supplies an optional pointer where the directory portion
        will be returned. This may be a pointer within the path or a static
        string.

    FileName - Supplies an optional pointer where the file name portion will be
        returned.

Return Value:

    None.

--*/

INT
MingenAddPathToList (
    PMINGEN_PATH_LIST PathList,
    PMINGEN_PATH Path
    );

/*++

Routine Description:

    This routine adds a path to the path list.

Arguments:

    PathList - Supplies a pointer to the path list to add to.

    Path - Supplies a pointer to the path to add.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

VOID
MingenDestroyPathList (
    PMINGEN_PATH_LIST PathList
    );

/*++

Routine Description:

    This routine destroys a path list, freeing all entries.

Arguments:

    PathList - Supplies a pointer to the path list.

Return Value:

    None.

--*/

VOID
MingenDeduplicatePathList (
    PMINGEN_PATH_LIST PathList
    );

/*++

Routine Description:

    This routine sorts and deduplicates a path list.

Arguments:

    PathList - Supplies a pointer to the path list.

Return Value:

    None.

--*/

INT
MingenCreateDirectories (
    PMINGEN_CONTEXT Context,
    PMINGEN_PATH_LIST PathList
    );

/*++

Routine Description:

    This routine creates directories, including intermediate directories,
    in the given path list. If creating one directory fails, the routine still
    tries to create the others.

Arguments:

    Context - Supplies a pointer to the application context.

    PathList - Supplies a pointer to the path list.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
MingenCreateDirectory (
    PSTR Path
    );

/*++

Routine Description:

    This routine creates a directory, including intermediate directories.

Arguments:

    Path - Supplies a pointer to the directory to create.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

PSTR
MingenGetAbsoluteDirectory (
    PSTR Path
    );

/*++

Routine Description:

    This routine converts the given path into an absolute path by changing to
    that directory.

Arguments:

    Path - Supplies a pointer to the directory path.

Return Value:

    Returns the absolute path of the given directory. It is the caller's
    responsibility to free this memory.

    NULL on allocation failure, or if the directory does not exist.

--*/

//
// Chalk support functions
//

INT
MingenAddChalkBuiltins (
    PMINGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine adds the functions in the global scope of the Chalk
    interpreter for the mingen program.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// OS-specific functions
//

INT
MingenOsUname (
    CHAR Flavor,
    PSTR Buffer,
    ULONG Size
    );

/*++

Routine Description:

    This routine implements the OS-specific uname function.

Arguments:

    Flavor - Supplies the flavor of uname to get. Valid values are s, n, r, v,
        and m.

    Buffer - Supplies a buffer where the string will be returned on success.

    Size - Supplies the size of the buffer in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

//
// Output generator functions
//

INT
MingenCreateMakefile (
    PMINGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine creates a Makefile out of the build graph.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
MingenCreateNinja (
    PMINGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine creates a Ninja build file out of the build graph.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/


/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    mbgen.h

Abstract:

    This header contains definitions for the Minoca Build Generator.

Author:

    Evan Green 3-Dec-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/types.h>
#include <minoca/status.h>
#include "chalk.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MBGEN_PROJECT_FILE ".mbproj"
#define MBGEN_BUILD_FILE "build.mb"

#define MBGEN_OPTION_VERBOSE 0x00000001
#define MBGEN_OPTION_DEBUG 0x00000002
#define MBGEN_OPTION_DRY_RUN 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MBGEN_DIRECTORY_TREE {
    MbgenDirectoryTreeInvalid,
    MbgenSourceTree,
    MbgenBuildTree,
    MbgenAbsolutePath,
} MBGEN_DIRECTORY_TREE, *PMBGEN_DIRECTORY_TREE;

/*++

Structure Description:

    This structure stores the global information for an instantiation of the
    Minoca Build Generator application.

Members:

    Options - Stores the bitfield of application options. see MBGEN_OPTION_*
        definitions.

    Interpreter - Stores the Chalk interpreter context.

    ProjectFileName - Stores a pointer to the project file name (just the name,
        not the directory).

    BuildFileName - Stores a pointer ot the build file name to look for in each
        directory.

    SourceRoot - Stores a pointer to the source root directory.

    BuildRoot - Stores a pointer to the build root directory.

    ScriptList - Stores the head of the list of loaded scripts.

--*/

typedef struct _MBGEN_CONTEXT {
    ULONG Options;
    CHALK_INTERPRETER Interpreter;
    PSTR ProjectFileName;
    PSTR BuildFileName;
    PSTR SourceRoot;
    PSTR BuildRoot;
    LIST_ENTRY ScriptList;
} MBGEN_CONTEXT, *PMBGEN_CONTEXT;

/*++

Structure Description:

    This structure stores the components of a fully specified build target path.

Members:

    Root - Stores the directory tree root for the target.

    Path - Stores the directory path relative to the root of the target.

    Target - Stores the target name.

    Toolchain - Stores the target toolchain.

--*/

typedef struct _MBGEN_TARGET_SPECIFIER {
    MBGEN_DIRECTORY_TREE Root;
    PSTR Path;
    PSTR Target;
    PSTR Toolchain;
} MBGEN_TARGET_SPECIFIER, *PMBGEN_TARGET_SPECIFIER;

/*++

Structure Description:

    This structure stores information about a loaded script.

Members:

    ListEntry - Stores pointers to the next and previous scripts in the build.

    Root - Stores the directory tree root for the target.

    Path - Stores the directory path relative to the root of the target, not
        including the actual file name.

    CompletePath - Stores the complete file path to the script.

    Script - Stores a pointer to the script contents.

    Size - Stores the size of the script file in bytes, not including the
        artificially appended null terminator.

    Result - Stores the result returned from executing the script.

--*/

typedef struct _MBGEN_SCRIPT {
    LIST_ENTRY ListEntry;
    MBGEN_DIRECTORY_TREE Root;
    PSTR Path;
    PSTR CompletePath;
    PSTR Script;
    UINTN Size;
    PCHALK_OBJECT Result;
} MBGEN_SCRIPT, *PMBGEN_SCRIPT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Script utility functions
//

INT
MbgenLoadProjectRoot (
    PMBGEN_CONTEXT Context
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
MbgenLoadScript (
    PMBGEN_CONTEXT Context,
    PMBGEN_TARGET_SPECIFIER TargetPath
    );

/*++

Routine Description:

    This routine loads and interprets a given target path. If the script
    containing the given target path is already loaded, then this is a no-op.

Arguments:

    Context - Supplies a pointer to the application context.

    TargetPath - Supplies a pointer to the target path to load. The target
        name and toolchain are ignored, only the root and path are observed.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

VOID
MbgenDestroyAllScripts (
    PMBGEN_CONTEXT Context
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

PSTR
MbgenAppendPaths3 (
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
MbgenAppendPaths (
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
MbgenSetupRootDirectories (
    PMBGEN_CONTEXT Context
    );

/*++

Routine Description:

    This routine finds or validates the source root directory, and validates
    the build directory.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

PSTR
MbgenPathForTree (
    PMBGEN_CONTEXT Context,
    MBGEN_DIRECTORY_TREE Tree
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


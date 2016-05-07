/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    script.c

Abstract:

    This module implements Chalk script support for the Minoca Build Generator
    tool.

Author:

    Evan Green 3-Dec-2015

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mingen.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
MingenDestroyScript (
    PMINGEN_SCRIPT Script
    );

//
// -------------------------------------------------------------------- Globals
//

CHALK_C_STRUCTURE_MEMBER MingenProjectRootMembers[] = {
    {
        ChalkCString,
        "globalenv",
        offsetof(MINGEN_CONTEXT, GlobalName),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "default_target",
        offsetof(MINGEN_CONTEXT, DefaultName),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "output_format",
        offsetof(MINGEN_CONTEXT, FormatString),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "default_build_dir",
        offsetof(MINGEN_CONTEXT, BuildRoot),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "build_file_name",
        offsetof(MINGEN_CONTEXT, BuildFileName),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "source_root",
        offsetof(MINGEN_CONTEXT, SourceRoot),
        FALSE,
        {0}
    },

    {0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
MingenLoadTargetScript (
    PMINGEN_CONTEXT Context,
    PMINGEN_PATH Target,
    PMINGEN_SCRIPT *Script
    )

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

{

    INT Status;

    Status = MingenLoadScript(Context, MingenScriptOrderTarget, Target, Script);
    if (Status != 0) {
        goto LoadTargetScriptEnd;
    }

LoadTargetScriptEnd:
    return Status;
}

INT
MingenLoadProjectRoot (
    PMINGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine loads and interprets the project root script.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR BuildRoot;
    INT Status;
    MINGEN_PATH TargetPath;

    Status = MingenAddChalkBuiltins(Context);
    if (Status != 0) {
        goto LoadProjectRootEnd;
    }

    memset(&TargetPath, 0, sizeof(MINGEN_PATH));
    TargetPath.Root = MingenAbsolutePath;
    TargetPath.Path = Context->ProjectFilePath;
    Status = MingenLoadScript(Context,
                              MingenScriptOrderProjectRoot,
                              &TargetPath,
                              NULL);

    if (Status != 0) {
        goto LoadProjectRootEnd;
    }

    if ((Context->Options & MINGEN_OPTION_DEBUG) != 0) {
        printf("Global context after project root:\n");
        ChalkPrintObject(stdout, Context->Interpreter.Global.Dict, 0);
        printf("\n");
    }

    //
    // Read the important variables into the context structure.
    //

    BuildRoot = Context->BuildRoot;
    Status = ChalkConvertDictToStructure(&(Context->Interpreter),
                                         Context->Interpreter.Global.Dict,
                                         MingenProjectRootMembers,
                                         Context);

    if (Status != 0) {
        goto LoadProjectRootEnd;
    }

    if ((BuildRoot != NULL) && (Context->BuildRoot != BuildRoot)) {
        if (Context->BuildRoot != NULL) {
            free(Context->BuildRoot);
        }

        Context->BuildRoot = BuildRoot;
    }

    Status = MingenFindSourceRoot(Context);
    if (Status != 0) {
        fprintf(stderr,
                "Error: Unable to determine source root directory: %s.\n",
                strerror(Status));

        goto LoadProjectRootEnd;
    }

    //
    // Make the build root the source root if no one asked for anything else.
    //

    if (Context->BuildRoot == NULL) {
        Context->BuildRoot = strdup(Context->SourceRoot);
        if (Context->BuildRoot == NULL) {
            Status = ENOMEM;
            goto LoadProjectRootEnd;
        }
    }

    //
    // The build root can be relative to the source root. Append the source
    // root path if so.
    //

    if (MINGEN_IS_SOURCE_ROOT_RELATIVE(Context->BuildRoot)) {
        BuildRoot = MingenAppendPaths(Context->SourceRoot,
                                      Context->BuildRoot + 2);

        if (BuildRoot == NULL) {
            Status = ENOMEM;
            goto LoadProjectRootEnd;
        }

        free(Context->BuildRoot);
        Context->BuildRoot = BuildRoot;
    }

    Status = MingenCreateDirectory(Context->BuildRoot);
    if (Status != 0) {
        goto LoadProjectRootEnd;
    }

    BuildRoot = MingenGetAbsoluteDirectory(Context->BuildRoot);
    if (BuildRoot == NULL) {
        Status = errno;
        fprintf(stderr,
                "Error: unable to get absolute directory of %s: %s.\n",
                Context->BuildRoot,
                strerror(Status));

        if (Status == 0) {
            Status = -1;
        }

        goto LoadProjectRootEnd;
    }

    free(Context->BuildRoot);
    Context->BuildRoot = BuildRoot;
    if ((Context->Options & MINGEN_OPTION_VERBOSE) != 0) {
        printf("Source Root: '%s'\nBuild Root: '%s'\n",
               Context->SourceRoot,
               Context->BuildRoot);
    }

    if (Context->DefaultName == NULL) {
        Context->DefaultName = strdup(MINGEN_DEFAULT_NAME);
        if (Context->DefaultName == NULL) {
            Status = ENOMEM;
            goto LoadProjectRootEnd;
        }
    }

    //
    // Re-initialize the interpreter for the target environment.
    //

    ChalkClearInterpreter(&(Context->Interpreter));
    Status = MingenAddChalkBuiltins(Context);
    if (Status != 0) {
        goto LoadProjectRootEnd;
    }

    //
    // Execute the command line arguments and global contents.
    //

    Status = ChalkExecuteDeferredScripts(&(Context->Interpreter),
                                         MingenScriptOrderCommandLine);

    if (Status != 0) {
        goto LoadProjectRootEnd;
    }

    //
    // Load up the global environment script to get it loaded with the correct
    // order.
    //

    if (Context->GlobalName != NULL) {
        Status = MingenParsePath(Context,
                                 Context->GlobalName,
                                 MingenSourceTree,
                                 NULL,
                                 &TargetPath);

        if (Status != 0) {
            goto LoadProjectRootEnd;
        }

        Status = MingenLoadScript(Context,
                                  MingenScriptOrderGlobal,
                                  &TargetPath,
                                  NULL);

        if (TargetPath.Path != NULL) {
            free(TargetPath.Path);
            TargetPath.Path = NULL;
        }

        if (Status != 0) {
            fprintf(stderr,
                    "Error: Failed to load global environment script.\n");

            goto LoadProjectRootEnd;
        }
    }

    //
    // Load the default target.
    //

    if (Context->DefaultName != NULL) {
        Status = MingenParsePath(Context,
                                 Context->DefaultName,
                                 MingenSourceTree,
                                 NULL,
                                 &TargetPath);

        if (Status != 0) {
            goto LoadProjectRootEnd;
        }

        Status = MingenLoadTargetScript(Context,
                                        &TargetPath,
                                        NULL);

        if (TargetPath.Path != NULL) {
            free(TargetPath.Path);
            TargetPath.Path = NULL;
        }

        if (Status != 0) {
            fprintf(stderr, "Error: Failed to load default target.\n");
            goto LoadProjectRootEnd;
        }
    }

    //
    // Get the default format.
    //

    if (Context->Format == MingenOutputInvalid) {
        if (Context->FormatString != NULL) {
            if (strcasecmp(Context->FormatString, "make") == 0) {
                Context->Format = MingenOutputMake;

            } else if (strcasecmp(Context->FormatString, "ninja") == 0) {
                Context->Format = MingenOutputNinja;

            } else if (strcasecmp(Context->FormatString, "none") == 0) {
                Context->Format = MingenOutputNone;

            } else {
                fprintf(stderr,
                        "Error: Unknown output format %s.\n",
                        Context->FormatString);

                Status = EINVAL;
                goto LoadProjectRootEnd;
            }
        }
    }

LoadProjectRootEnd:
    return Status;
}

INT
MingenLoadScript (
    PMINGEN_CONTEXT Context,
    MINGEN_SCRIPT_ORDER Order,
    PMINGEN_PATH TargetPath,
    PMINGEN_SCRIPT *FinalScript
    )

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

{

    PSTR BuildFileName;
    ULONG ExecuteOrder;
    FILE *File;
    PSTR FinalPath;
    PMINGEN_SCRIPT Script;
    struct stat Stat;
    INT Status;
    PSTR Tree;

    File = NULL;
    FinalPath = NULL;
    Script = NULL;
    Script = MingenFindScript(Context, TargetPath);
    if (Script != NULL) {
        Status = 0;
        goto LoadScriptEnd;
    }

    Tree = MingenPathForTree(Context, TargetPath->Root);
    if ((Order == MingenScriptOrderGlobal) ||
        (Order == MingenScriptOrderProjectRoot)) {

        FinalPath = MingenAppendPaths(Tree, TargetPath->Path);

    } else {
        BuildFileName = Context->BuildFileName;
        if (BuildFileName == NULL) {
            BuildFileName = MINGEN_BUILD_FILE;
        }

        FinalPath = MingenAppendPaths3(Tree,
                                       TargetPath->Path,
                                       BuildFileName);
    }

    if (FinalPath == NULL) {
        return ENOMEM;
    }

    if ((Context->Options & MINGEN_OPTION_VERBOSE) != 0) {
        printf("Loading Script %s\n", FinalPath);
    }

    //
    // Load the script.
    //

    File = fopen(FinalPath, "rb");
    if (File == NULL) {
        Status = errno;
        fprintf(stderr,
                "Unable to open %s: %s\n",
                FinalPath,
                strerror(Status));

        goto LoadScriptEnd;
    }

    if (stat(FinalPath, &Stat) != 0) {
        Status = errno;
        goto LoadScriptEnd;
    }

    Script = malloc(sizeof(MINGEN_SCRIPT));
    if (Script == NULL) {
        Status = errno;
        goto LoadScriptEnd;
    }

    memset(Script, 0, sizeof(MINGEN_SCRIPT));
    INITIALIZE_LIST_HEAD(&(Script->TargetList));
    Script->Order = Order;
    Script->Root = TargetPath->Root;
    Script->CompletePath = FinalPath;
    if (TargetPath->Path != NULL) {
        Script->Path = strdup(TargetPath->Path);
        if (Script->Path == NULL) {
            Status = errno;
            goto LoadScriptEnd;
        }
    }

    Script->Size = Stat.st_size;
    Script->Script = malloc(Script->Size + 1);
    if (Script->Script == NULL) {
        Status = errno;
        goto LoadScriptEnd;
    }

    if (fread(Script->Script, 1, Script->Size, File) != Script->Size) {
        fprintf(stderr, "Read failure");
        Status = errno;
        goto LoadScriptEnd;
    }

    Script->Script[Script->Size] = '\0';

    //
    // If no specific targets were requested, then all scripts are active.
    //

    if (Context->RequestedTargetCount == 0) {
        Script->Flags |= MINGEN_SCRIPT_ACTIVE;
    }

    //
    // Execute the script. If it's a target script, execute now to get the
    // return value.
    //

    ExecuteOrder = Order;
    if (Order == MingenScriptOrderTarget) {
        ExecuteOrder = 0;
    }

    Status = ChalkLoadScriptBuffer(&(Context->Interpreter),
                                   FinalPath,
                                   Script->Script,
                                   Script->Size,
                                   ExecuteOrder,
                                   &(Script->Result));

    if (Status != 0) {
        fprintf(stderr,
                "Error: Failed to execute script %s: %s.\n",
                FinalPath,
                strerror(Status));

        goto LoadScriptEnd;
    }

    if (ExecuteOrder != 0) {
        Status = ChalkExecuteDeferredScripts(&(Context->Interpreter), Order);
        if (Status != 0) {
            goto LoadScriptEnd;
        }

        INSERT_BEFORE(&(Script->ListEntry), &(Context->ScriptList));

    } else {
        INSERT_BEFORE(&(Script->ListEntry), &(Context->ScriptList));
        Status = MingenParseScriptResults(Context, Script);
        if (Status != 0) {
            goto LoadScriptEnd;
        }
    }

    Status = 0;

LoadScriptEnd:
    if ((FinalPath != NULL) && (Script == NULL)) {
        free(FinalPath);
    }

    if (File != NULL) {
        fclose(File);
    }

    if (Status != 0) {
        if (Script != NULL) {
            if (Script->ListEntry.Next != NULL) {
                LIST_REMOVE(&(Script->ListEntry));
            }

            MingenDestroyScript(Script);
            Script = NULL;
        }
    }

    if (FinalScript != NULL) {
        *FinalScript = Script;
    }

    return Status;
}

VOID
MingenDestroyAllScripts (
    PMINGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys all scripts in the application context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PMINGEN_SCRIPT Script;

    while (!LIST_EMPTY(&(Context->ScriptList))) {
        Script = LIST_VALUE(Context->ScriptList.Next,
                            MINGEN_SCRIPT,
                            ListEntry);

        LIST_REMOVE(&(Script->ListEntry));
        MingenDestroyScript(Script);
    }

    return;
}

PMINGEN_SCRIPT
MingenFindScript (
    PMINGEN_CONTEXT Context,
    PMINGEN_PATH TargetPath
    )

/*++

Routine Description:

    This routine searches for an already loaded script matching the given
    target root and directory path.

Arguments:

    Context - Supplies a pointer to the application context.

    TargetPath - Supplies a pointer to the target path search for. The target
        name and toolchain are ignored, only the root and path are observed.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMINGEN_SCRIPT Script;

    CurrentEntry = Context->ScriptList.Next;
    while (CurrentEntry != &(Context->ScriptList)) {
        Script = LIST_VALUE(CurrentEntry, MINGEN_SCRIPT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Script->Root == TargetPath->Root) &&
            (Script->Path != NULL) &&
            (strcmp(Script->Path, TargetPath->Path) == 0)) {

            return Script;
        }
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
MingenDestroyScript (
    PMINGEN_SCRIPT Script
    )

/*++

Routine Description:

    This routine destroys a script object.

Arguments:

    Script - Supplies a pointer to the script.

Return Value:

    None.

--*/

{

    PMINGEN_TARGET Target;

    if (Script->Script != NULL) {
        free(Script->Script);
    }

    if (Script->Path != NULL) {
        free(Script->Path);
    }

    if (Script->CompletePath != NULL) {
        free(Script->CompletePath);
    }

    if (Script->Result != NULL) {
        ChalkObjectReleaseReference(Script->Result);
    }

    while (!LIST_EMPTY(&(Script->TargetList))) {
        Target = LIST_VALUE(Script->TargetList.Next, MINGEN_TARGET, ListEntry);
        LIST_REMOVE(&(Target->ListEntry));
        Script->TargetCount -= 1;
        MingenDestroyTarget(Target);
    }

    assert(Script->TargetCount == 0);

    free(Script);
    return;
}


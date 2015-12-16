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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mbgen.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PMBGEN_SCRIPT
MbgenFindScript (
    PMBGEN_CONTEXT Context,
    PMBGEN_TARGET_SPECIFIER TargetPath
    );

VOID
MbgenDestroyScript (
    PMBGEN_SCRIPT Script
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
MbgenLoadProjectRoot (
    PMBGEN_CONTEXT Context
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

    INT Status;
    MBGEN_TARGET_SPECIFIER TargetPath;

    memset(&TargetPath, 0, sizeof(MBGEN_TARGET_SPECIFIER));
    TargetPath.Root = MbgenSourceTree;
    Status = MbgenLoadScript(Context, &TargetPath);
    if (Status != 0) {
        return Status;
    }

    if ((Context->Options & MBGEN_OPTION_DEBUG) != 0) {
        printf("Global context after project root:\n");
        ChalkPrintObject(Context->Interpreter.Global.Dict, 0);
    }

    return 0;
}

INT
MbgenLoadScript (
    PMBGEN_CONTEXT Context,
    PMBGEN_TARGET_SPECIFIER TargetPath
    )

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

{

    FILE *File;
    PSTR FinalPath;
    PMBGEN_SCRIPT Script;
    struct stat Stat;
    INT Status;
    PSTR Tree;

    File = NULL;
    FinalPath = NULL;
    Script = NULL;

    //
    // If the path is NULL, then that special case is treated as the project
    // root script. Skip searching for an existing script in that case as the
    // string compare would fault on NULL.
    //

    if (TargetPath->Path == NULL) {

        assert((TargetPath->Root == MbgenSourceTree) &&
               (LIST_EMPTY(&(Context->ScriptList))));

        FinalPath = MbgenAppendPaths(Context->SourceRoot,
                                     Context->ProjectFileName);

    } else {
        if (MbgenFindScript(Context, TargetPath) != NULL) {
            return 0;
        }

        Tree = MbgenPathForTree(Context, TargetPath->Root);
        FinalPath = MbgenAppendPaths3(Tree,
                                      TargetPath->Path,
                                      Context->BuildFileName);
    }

    if (FinalPath == NULL) {
        return ENOMEM;
    }

    if ((Context->Options & MBGEN_OPTION_VERBOSE) != 0) {
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

    Script = malloc(sizeof(MBGEN_SCRIPT));
    if (Script == NULL) {
        Status = errno;
        goto LoadScriptEnd;
    }

    memset(Script, 0, sizeof(MBGEN_SCRIPT));
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
    // Execute the script.
    //

    Status = ChalkLoadScriptBuffer(&(Context->Interpreter),
                                   FinalPath,
                                   Script->Script,
                                   Script->Size,
                                   0,
                                   &(Script->Result));

    if (Status != 0) {
        fprintf(stderr,
                "Error: Failed to execute script %s: %s.\n",
                FinalPath,
                strerror(Status));

        goto LoadScriptEnd;
    }

    if ((Context->Options & MBGEN_OPTION_DEBUG) != 0) {
        ChalkPrintObject(Script->Result, 0);
    }

    INSERT_BEFORE(&(Script->ListEntry), &(Context->ScriptList));
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
            MbgenDestroyScript(Script);
        }
    }

    return Status;
}

VOID
MbgenDestroyAllScripts (
    PMBGEN_CONTEXT Context
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

    PMBGEN_SCRIPT Script;

    while (!LIST_EMPTY(&(Context->ScriptList))) {
        Script = LIST_VALUE(Context->ScriptList.Next,
                            MBGEN_SCRIPT,
                            ListEntry);

        LIST_REMOVE(&(Script->ListEntry));
        MbgenDestroyScript(Script);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PMBGEN_SCRIPT
MbgenFindScript (
    PMBGEN_CONTEXT Context,
    PMBGEN_TARGET_SPECIFIER TargetPath
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
    PMBGEN_SCRIPT Script;

    CurrentEntry = Context->ScriptList.Next;
    while (CurrentEntry != &(Context->ScriptList)) {
        Script = LIST_VALUE(CurrentEntry, MBGEN_SCRIPT, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((Script->Root == TargetPath->Root) &&
            (strcmp(Script->Path, TargetPath->Path) == 0)) {

            return Script;
        }
    }

    return NULL;
}

VOID
MbgenDestroyScript (
    PMBGEN_SCRIPT Script
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

    free(Script);
    return;
}


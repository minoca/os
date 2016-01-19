/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    mbgen.c

Abstract:

    This module implements support for the Minoca Build Generator utility,
    which takes build descriptions and generates Ninja build files.

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
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mbgen.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MBGEN_VERSION_MAJOR 1
#define MBGEN_VERSION_MINOR 0

#define MBGEN_USAGE                                                            \
    "usage: mbgen [options] build_dir\n"                               \
    "The Minoca Build Generator creates Ninja files describing the build at \n"\
    "the current directory. Options are:\n" \
    "  -a, --args=expr -- Evaluate the given text in the script interpreter \n"\
    "      context before loading the project root file. This can be used \n" \
    "      to pass configuration arguments and overrides to the build.\n" \
    "      This can be specified multiple times.\n" \
    "  -D, --debug -- Print lots of information during execution.\n" \
    "  -B, --build-file=file_name -- Use the given file as the name of the \n" \
    "      build files, rather than the default, build.mb." \
    "  -n, --dry-run -- Do all the processing, but do not actually create \n" \
    "      any output files.\n" \
    "  -p, --project=file_name -- Search for the given file name when \n" \
    "      looking for the project root file. The default is \".mbproj\".\n" \
    "  -r, --root=directory -- Explictly set the project source root. If \n" \
    "      not specified, then the project file will be searched up the \n" \
    "      current directory hierarchy.\n" \
    "  -v, --verbose -- Print more information during processing.\n" \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n\n"   \

#define MBGEN_OPTIONS_STRING "B:Dhnp:r:vV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
MbgenInitializeContext (
    PMBGEN_CONTEXT Context
    );

VOID
MbgenDestroyContext (
    PMBGEN_CONTEXT Context
    );

INT
MbgenParseToolEntry (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    );

INT
MbgenParseTargetEntry (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    );

INT
MbgenProcessEntries (
    PMBGEN_CONTEXT Context
    );

INT
MbgenProcessTool (
    PMBGEN_CONTEXT Context,
    PMBGEN_TOOL Tool
    );

INT
MbgenProcessTarget (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_TARGET Target
    );

INT
MbgenAddTargetsToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_TARGET_LIST TargetList,
    PCHALK_OBJECT List
    );

INT
MbgenAddTargetToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_TARGET_LIST TargetList,
    PSTR Name
    );

PMBGEN_TOOL
MbgenFindTool (
    PMBGEN_CONTEXT Context,
    PSTR Name
    );

PMBGEN_TARGET
MbgenFindTargetInScript (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PSTR Name
    );

VOID
MbgenDestroyTool (
    PMBGEN_TOOL Tool
    );

VOID
MbgenPrintAllEntries (
    PMBGEN_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

struct option MbgenLongOptions[] = {
    {"args", required_argument, 0, 'a'},
    {"build-file", required_argument, 0, 'B'},
    {"debug", no_argument, 0, 'D'},
    {"dry-run", no_argument, 0, 'n'},
    {"project", required_argument, 0, 'p'},
    {"root", required_argument, 0, 'r'},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

CHALK_C_STRUCTURE_MEMBER MbgenToolMembers[] = {
    {
        ChalkCString,
        "name",
        offsetof(MBGEN_TOOL, Name),
        TRUE,
        {0}
    },

    {
        ChalkCString,
        "command",
        offsetof(MBGEN_TOOL, Command),
        TRUE,
        {0}
    },

    {
        ChalkCString,
        "description",
        offsetof(MBGEN_TOOL, Description),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "depfile",
        offsetof(MBGEN_TOOL, Depfile),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "depsformat",
        offsetof(MBGEN_TOOL, DepsFormat),
        FALSE,
        {0}
    },

    {0}
};

CHALK_C_STRUCTURE_MEMBER MbgenTargetMembers[] = {
    {
        ChalkCString,
        "label",
        offsetof(MBGEN_TARGET, Label),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "output",
        offsetof(MBGEN_TARGET, Output),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "sources",
        offsetof(MBGEN_TARGET, SourcesList),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "tool",
        offsetof(MBGEN_TARGET, Tool),
        FALSE,
        {0}
    },

    {
        ChalkCFlag32,
        "phony",
        offsetof(MBGEN_TARGET, Flags),
        FALSE,
        {MBGEN_TARGET_PHONY}
    },

    {
        ChalkCObjectPointer,
        "deps",
        offsetof(MBGEN_TARGET, DepsList),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "public_deps",
        offsetof(MBGEN_TARGET, PublicDepsList),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "config",
        offsetof(MBGEN_TARGET, Config),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "public_config",
        offsetof(MBGEN_TARGET, PublicConfig),
        FALSE,
        {0}
    },

    {0}
};

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the Minoca Build Generator mode program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT ArgumentIndex;
    MBGEN_CONTEXT Context;
    INT Option;
    INT Status;

    srand(time(NULL) ^ getpid());
    Status = MbgenInitializeContext(&Context);
    if (Status != 0) {
        goto mainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MBGEN_OPTIONS_STRING,
                             MbgenLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = -1;
            goto mainEnd;
        }

        switch (Option) {
        case 'a':
            Status = ChalkLoadScriptBuffer(&(Context.Interpreter),
                                           "<cmdline>",
                                           optarg,
                                           strlen(optarg) + 1,
                                           MbgenScriptOrderCommandLine,
                                           NULL);

            if (Status == 0) {
                Status = ChalkExecuteDeferredScripts(
                                                  &(Context.Interpreter),
                                                  MbgenScriptOrderCommandLine);
            }

            if (Status != 0) {
                fprintf(stderr,
                        "Error: Bad command line arguments script: %s\n",
                        optarg);

                goto mainEnd;
            }

            break;

        case 'B':
            Context.BuildFileName = optarg;
            if (strchr(optarg, '/') != NULL) {
                fprintf(stderr,
                        "Error: Build file should just be a file name, not a "
                        "path.\n");

                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'D':
            Context.Options |= MBGEN_OPTION_DEBUG;
            break;

        case 'n':
            Context.Options |= MBGEN_OPTION_DRY_RUN;
            break;

        case 'P':
            Context.ProjectFileName = optarg;
            if (strchr(optarg, '/') != NULL) {
                fprintf(stderr,
                        "Error: Project file should just be a file name, not a "
                        "path.\n");

                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'r':
            Context.SourceRoot = strdup(optarg);
            break;

        case 'v':
            Context.Options |= MBGEN_OPTION_VERBOSE;
            break;

        case 'V':
            printf("Minoca build generator version %d.%d.%d\n"
                   "Built on %s\n"
                   "Copyright (c) 2015 Minoca Corp. All Rights Reserved.\n\n",
                   MBGEN_VERSION_MAJOR,
                   MBGEN_VERSION_MINOR,
                   REVISION,
                   BUILD_TIME_STRING);

            return 1;

        case 'h':
            printf(MBGEN_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto mainEnd;
        }
    }

    //
    // Set up the source root and the build root.
    //

    ArgumentIndex = optind;
    if (ArgumentIndex == ArgumentCount - 1) {
        Context.BuildRoot = strdup(Arguments[ArgumentIndex]);

    } else if (ArgumentIndex < ArgumentCount) {
        fprintf(stderr, "Too many arguments. Try --help for usage.\n");
        Status = EINVAL;
        goto mainEnd;

    } else {
        if ((Context.Options & MBGEN_OPTION_DRY_RUN) == 0) {
            fprintf(stderr, "Argument expected. Try --help for usage.\n");
            Status = EINVAL;
            goto mainEnd;

        } else {
            Context.BuildRoot = strdup("/");
        }
    }

    Status = MbgenSetupRootDirectories(&Context);
    if (Status != 0) {
        goto mainEnd;
    }

    if ((Context.Options & MBGEN_OPTION_VERBOSE) != 0) {
        printf("Source Root: '%s'\nBuild Root: '%s'\n",
               Context.SourceRoot,
               Context.BuildRoot);
    }

    //
    // Load the project root file. This also loads the default target file.
    //

    Status = MbgenLoadProjectRoot(&Context);
    if (Status != 0) {
        fprintf(stderr, "Failed to load project root: %s.\n", strerror(Status));
        goto mainEnd;
    }

    //
    // Process the targets, which may cause more targets to get loaded.
    //

    Status = MbgenProcessEntries(&Context);
    if (Status != 0) {
        goto mainEnd;
    }

    if ((Context.Options & MBGEN_OPTION_VERBOSE) != 0) {
        printf("Entries:\n");
        MbgenPrintAllEntries(&Context);
        printf("\n");
    }

mainEnd:
    MbgenDestroyContext(&Context);
    if (Status != 0) {
        fprintf(stderr,
                "mbgen exiting with status %d: %s\n",
                Status,
                strerror(Status));
    }

    return Status;
}

INT
MbgenParseScriptResults (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script
    )

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

{

    PCHALK_OBJECT Entry;
    ULONG Index;
    PCHALK_OBJECT List;
    INT Status;
    PCHALK_OBJECT Type;

    List = Script->Result;
    if ((Context->Options & MBGEN_OPTION_DEBUG) != 0) {
        ChalkPrintObject(List, 0);
        printf("\n");
    }

    if (List->Header.Type != ChalkObjectList) {
        fprintf(stderr,
                "Error: Script %s didn't return a list.\n",
                Script->CompletePath);

        return EINVAL;
    }

    for (Index = 0; Index < List->List.Count; Index += 1) {
        Entry = List->List.Array[Index];
        if (Entry == NULL) {
            continue;
        }

        if (Entry->Header.Type != ChalkObjectDict) {
            fprintf(stderr,
                    "Error: Script %s, element %d result not a dictionary.\n",
                    Script->CompletePath,
                    Index);

            Status = EINVAL;
            goto ParseScriptResultsEnd;
        }

        Type = ChalkDictLookupCStringKey(Entry, "type");
        if ((Type != NULL) && (Type->Header.Type != ChalkObjectString)) {
            fprintf(stderr,
                    "Error: Script %s, element %d type not a string.\n",
                    Script->CompletePath,
                    Index);

            Status = EINVAL;
            goto ParseScriptResultsEnd;
        }

        if ((Type == NULL) ||
            (strcasecmp(Type->String.String, "target") == 0)) {

            Status = MbgenParseTargetEntry(Context, Script, Entry);

        } else if (strcasecmp(Type->String.String, "tool") == 0) {
            Status = MbgenParseToolEntry(Context, Script, Entry);

        } else {
            fprintf(stderr,
                    "Error: Script %s, element %d type %s not valid.\n",
                    Script->CompletePath,
                    Index,
                    Type->String.String);

            Status = EINVAL;
            goto ParseScriptResultsEnd;
        }

        if (Status != 0) {
            fprintf(stderr,
                    "Error: Failed to parse script %s, result %d.\n",
                    Script->CompletePath,
                    Index);

            goto ParseScriptResultsEnd;
        }
    }

    Status = 0;

ParseScriptResultsEnd:
    return Status;
}

VOID
MbgenDestroyTarget (
    PMBGEN_TARGET Target
    )

/*++

Routine Description:

    This routine destroys a target entry.

Arguments:

    Target - Supplies a pointer to the target to destroy.

Return Value:

    None.

--*/

{

    if (Target->Label != NULL) {
        free(Target->Label);
    }

    if (Target->Output != NULL) {
        free(Target->Output);
    }

    if (Target->Tool != NULL) {
        free(Target->Tool);
    }

    if (Target->Deps.List != NULL) {
        free(Target->Deps.List);
    }

    if (Target->PublicDeps.List != NULL) {
        free(Target->PublicDeps.List);
    }

    free(Target);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
MbgenInitializeContext (
    PMBGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the mbgen context.

Arguments:

    Context - Supplies a pointer to the context to initialize.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    memset(Context, 0, sizeof(MBGEN_CONTEXT));
    INITIALIZE_LIST_HEAD(&(Context->ScriptList));
    INITIALIZE_LIST_HEAD(&(Context->ToolList));
    ChalkInitializeInterpreter(&(Context->Interpreter));
    Context->ProjectFileName = MBGEN_PROJECT_FILE;
    Context->BuildFileName = MBGEN_BUILD_FILE;
    return 0;
}

VOID
MbgenDestroyContext (
    PMBGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys an mbgen context.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

{

    PMBGEN_TOOL Tool;

    MbgenDestroyAllScripts(Context);
    while (!LIST_EMPTY(&(Context->ToolList))) {
        Tool = LIST_VALUE(Context->ToolList.Next, MBGEN_TOOL, ListEntry);
        LIST_REMOVE(&(Tool->ListEntry));
        MbgenDestroyTool(Tool);
    }

    if (Context->SourceRoot != NULL) {
        free(Context->SourceRoot);
        Context->SourceRoot = NULL;
    }

    if (Context->BuildRoot != NULL) {
        free(Context->BuildRoot);
        Context->BuildRoot = NULL;
    }

    if (Context->GlobalName != NULL) {
        free(Context->GlobalName);
        Context->GlobalName = NULL;
    }

    if (Context->DefaultName != NULL) {
        free(Context->DefaultName);
        Context->DefaultName = NULL;
    }

    ChalkDestroyInterpreter(&(Context->Interpreter));
    return;
}

INT
MbgenParseToolEntry (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    )

/*++

Routine Description:

    This routine parses a new tool entry.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the script being parsed.

    Entry - Supplies a pointer to the tool entry.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;
    PMBGEN_TOOL Tool;

    Tool = malloc(sizeof(MBGEN_TOOL));
    if (Tool == NULL) {
        return ENOMEM;
    }

    memset(Tool, 0, sizeof(MBGEN_TOOL));
    Status = ChalkConvertDictToStructure(&(Context->Interpreter),
                                         Entry,
                                         MbgenToolMembers,
                                         Tool);

    if (Status != 0) {
        goto ParseToolEntryEnd;
    }

    if (MbgenFindTool(Context, Tool->Name) != NULL) {
        fprintf(stderr, "Error: Duplicate tool %s.\n", Tool->Name);
        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    INSERT_BEFORE(&(Tool->ListEntry), &(Context->ToolList));
    Status = 0;

ParseToolEntryEnd:
    if (Status != 0) {
        if (Tool != NULL) {
            MbgenDestroyTool(Tool);
        }
    }

    return Status;
}

INT
MbgenParseTargetEntry (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    )

/*++

Routine Description:

    This routine parses a new target entry.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the script being parsed.

    Entry - Supplies a pointer to the target entry.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;
    PMBGEN_TARGET Target;

    Target = malloc(sizeof(MBGEN_TARGET));
    if (Target == NULL) {
        return ENOMEM;
    }

    memset(Target, 0, sizeof(MBGEN_TARGET));
    Target->Script = Script;
    Status = ChalkConvertDictToStructure(&(Context->Interpreter),
                                         Entry,
                                         MbgenTargetMembers,
                                         Target);

    if (Status != 0) {
        goto ParseToolEntryEnd;
    }

    //
    // At least one of the output or label must be specified.
    //

    if ((Target->Label == NULL) && (Target->Output == NULL)) {
        fprintf(stderr, "Error: label or output must be defined.\n");
        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    if (Target->Label == NULL) {
        Target->Label = strdup(Target->Output);

    } else if (Target->Output == NULL) {
        Target->Output = strdup(Target->Label);
    }

    if ((Target->Label == NULL) || (Target->Output == NULL)) {
        Status = ENOMEM;
        goto ParseToolEntryEnd;
    }

    //
    // The label must be unique within the script.
    //

    if (MbgenFindTargetInScript(Context, Script, Target->Label) != NULL) {
        fprintf(stderr,
                "Error: Duplicate target %s:%s.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    //
    // The sources, deps, and public_deps if present must be lists.
    //

    if ((Target->SourcesList != NULL) &&
        (Target->SourcesList->Header.Type != ChalkObjectList)) {

        fprintf(stderr,
                "Error: sources for %s:%s must be a list.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    if ((Target->DepsList != NULL) &&
        (Target->DepsList->Header.Type != ChalkObjectList)) {

        fprintf(stderr,
                "Error: deps for %s:%s must be a list.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    if ((Target->PublicDepsList != NULL) &&
        (Target->PublicDepsList->Header.Type != ChalkObjectList)) {

        fprintf(stderr,
                "Error: public_deps for %s:%s must be a list.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    //
    // The config and public_config members if present must be dictionaries.
    //

    if ((Target->Config != NULL) &&
        (Target->Config->Header.Type != ChalkObjectDict)) {

        fprintf(stderr,
                "Error: config for %s:%s must be a dict.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    if ((Target->PublicConfig != NULL) &&
        (Target->PublicConfig->Header.Type != ChalkObjectDict)) {

        fprintf(stderr,
                "Error: public_config for %s:%s must be a dict.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    INSERT_BEFORE(&(Target->ListEntry), &(Script->TargetList));
    Script->TargetCount += 1;
    Status = 0;

ParseToolEntryEnd:
    if (Status != 0) {
        if (Target != NULL) {
            MbgenDestroyTarget(Target);
        }
    }

    return Status;
}

INT
MbgenProcessEntries (
    PMBGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine processes the dependency graph of entries, performing
    conversions from target names to output file names, and loading
    dependencies for targets that are referenced but not loaded.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PMBGEN_SCRIPT Script;
    PLIST_ENTRY ScriptEntry;
    INT Status;
    PMBGEN_TARGET Target;
    PLIST_ENTRY TargetEntry;
    PMBGEN_TOOL Tool;
    PLIST_ENTRY ToolEntry;

    Status = ENOENT;

    //
    // Iterate through all the scripts and all the targets in each script.
    // More scripts may get added onto the end of the list, but the list
    // iteration is safe since entries are never removed.
    //

    ScriptEntry = Context->ScriptList.Next;
    while (ScriptEntry != &(Context->ScriptList)) {
        Script = LIST_VALUE(ScriptEntry, MBGEN_SCRIPT, ListEntry);
        TargetEntry = Script->TargetList.Next;
        while (TargetEntry != &(Script->TargetList)) {
            Target = LIST_VALUE(TargetEntry, MBGEN_TARGET, ListEntry);
            Status = MbgenProcessTarget(Context, Script, Target);
            if (Status != 0) {
                fprintf(stderr,
                        "Failed to process %s:%s.\n",
                        Script->CompletePath,
                        Target->Label);

                goto ProcessEntriesEnd;
            }

            TargetEntry = TargetEntry->Next;
        }

        ScriptEntry = ScriptEntry->Next;
    }

    if (Status == ENOENT) {
        fprintf(stderr, "No targets were found.\n");
        goto ProcessEntriesEnd;
    }

    //
    // Process all the tools as well.
    //

    ToolEntry = Context->ToolList.Next;
    while (ToolEntry != &(Context->ToolList)) {
        Tool = LIST_VALUE(ToolEntry, MBGEN_TOOL, ListEntry);
        Status = MbgenProcessTool(Context, Tool);
        if (Status != 0) {
            fprintf(stderr, "Failed to process tool %s.\n", Tool->Name);
            goto ProcessEntriesEnd;
        }

        ToolEntry = ToolEntry->Next;
    }

    Status = 0;

ProcessEntriesEnd:
    return Status;
}

INT
MbgenProcessTool (
    PMBGEN_CONTEXT Context,
    PMBGEN_TOOL Tool
    )

/*++

Routine Description:

    This routine processes a tool entry, preparing it for output.

Arguments:

    Context - Supplies a pointer to the context.

    Tool - Supplies a pointer to the tool.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    return 0;
}

INT
MbgenProcessTarget (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_TARGET Target
    )

/*++

Routine Description:

    This routine processes a target entry, resolving all dependencies.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the script being parsed.

    Target - Supplies a pointer to the target to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PCHALK_OBJECT List;
    INT Status;

    //
    // Convert the deps to their pointers, loading them if needed.
    //

    List = Target->DepsList;
    if (List != NULL) {

        assert(Target->Deps.List == NULL);

        Status = MbgenAddTargetsToList(Context, Script, &(Target->Deps), List);
        if (Status != 0) {
            goto ProcessTargetEnd;
        }
    }

    //
    // Load and find all the public dependencies as well.
    //

    List = Target->PublicDepsList;
    if (List != NULL) {

        assert(Target->PublicDeps.List == NULL);

        Status = MbgenAddTargetsToList(Context,
                                       Script,
                                       &(Target->PublicDeps),
                                       List);

        if (Status != 0) {
            goto ProcessTargetEnd;
        }
    }

    Status = 0;

ProcessTargetEnd:
    return Status;
}

INT
MbgenAddTargetsToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_TARGET_LIST TargetList,
    PCHALK_OBJECT List
    )

/*++

Routine Description:

    This routine adds the targets described by the given list to the target
    list.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the current script.

    TargetList - Supplies a pointer to the target list to add to.

    List - Supplies a pointer to the target list.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONG Index;
    INT Status;
    PCHALK_OBJECT String;

    assert(List->Header.Type == ChalkObjectList);

    for (Index = 0; Index < List->List.Count; Index += 1) {
        String = List->List.Array[Index];
        if (String == NULL) {
            continue;
        }

        if (String->Header.Type != ChalkObjectString) {
            fprintf(stderr,
                    "Error: %s: dependency must be a string.\n",
                    Script->CompletePath);

            Status = EINVAL;
            goto AddTargetsToListEnd;
        }

        Status = MbgenAddTargetToList(Context,
                                      Script,
                                      TargetList,
                                      String->String.String);

        if (Status != 0) {
            fprintf(stderr,
                    "Error: %s: failed to add dependency %s: %s.\n",
                    Script->CompletePath,
                    String->String.String,
                    strerror(Status));

            goto AddTargetsToListEnd;
        }
    }

    Status = 0;

AddTargetsToListEnd:
    return Status;
}

INT
MbgenAddTargetToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_TARGET_LIST TargetList,
    PSTR Name
    )

/*++

Routine Description:

    This routine adds the targets described by the given name to the target
    list.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the current script.

    TargetList - Supplies a pointer to the target list to add to.

    Name - Supplies a pointer to the tool name of the target to add to the list.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PVOID NewBuffer;
    ULONG NewCapacity;
    INT Status;
    PMBGEN_TARGET Target;
    PSTR TargetName;
    PMBGEN_SCRIPT TargetScript;

    if (*Name == ':') {
        TargetScript = Script;

    } else {
        Status = MbgenLoadTargetScript(Context,
                                       Name,
                                       MbgenScriptOrderTarget,
                                       &TargetScript);

        if (Status != 0) {
            goto AddTargetToListEnd;
        }
    }

    TargetName = strrchr(Name, ':');

    //
    // Make sure the array is big enough.
    //

    NewCapacity = TargetList->Capacity;
    if (NewCapacity == 0) {
        NewCapacity = 16;
    }

    if ((TargetName == NULL) || (TargetName[1] == '\0')) {
        while (NewCapacity < TargetList->Count + TargetScript->TargetCount) {
            NewCapacity *= 2;
        }

    } else {
        while (NewCapacity < TargetList->Count) {
            NewCapacity *= 2;
        }
    }

    if (NewCapacity > TargetList->Capacity) {
        NewBuffer = realloc(TargetList->List, NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            Status = ENOMEM;
            goto AddTargetToListEnd;
        }

        TargetList->List = NewBuffer;
        TargetList->Capacity = NewCapacity;
    }

    //
    // Add all targets from the given script.
    //

    if ((TargetName == NULL) || (TargetName[1] == '\0')) {
        CurrentEntry = TargetScript->TargetList.Next;
        while (CurrentEntry != &(TargetScript->TargetList)) {
            Target = LIST_VALUE(CurrentEntry, MBGEN_TARGET, ListEntry);
            TargetList->List[TargetList->Count] = Target;
            TargetList->Count += 1;
            CurrentEntry = CurrentEntry->Next;
        }

    //
    // Add the specified target.
    //

    } else {
        TargetName += 1;
        Target = MbgenFindTargetInScript(Context, TargetScript, TargetName);
        if (Target == NULL) {
            fprintf(stderr,
                    "Error: Failed to find target %s:%s.\n",
                    TargetScript->CompletePath,
                    TargetName);

            Status = ENOENT;
            goto AddTargetToListEnd;
        }
    }

    Status = 0;

AddTargetToListEnd:
    return Status;
}

PMBGEN_TOOL
MbgenFindTool (
    PMBGEN_CONTEXT Context,
    PSTR Name
    )

/*++

Routine Description:

    This routine attempts to find a tool with the given name.

Arguments:

    Context - Supplies a pointer to the context.

    Name - Supplies a pointer to the tool name to find.

Return Value:

    Returns a pointer to the tool on success.

    NULL if no tool with the given name could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMBGEN_TOOL Tool;

    CurrentEntry = Context->ToolList.Next;
    while (CurrentEntry != &(Context->ToolList)) {
        Tool = LIST_VALUE(CurrentEntry, MBGEN_TOOL, ListEntry);
        if (strcmp(Tool->Name, Name) == 0) {
            return Tool;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

PMBGEN_TARGET
MbgenFindTargetInScript (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PSTR Name
    )

/*++

Routine Description:

    This routine attempts to find a target with the given name in the given
    script.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the script to search in.

    Name - Supplies a pointer to the target name to find.

Return Value:

    Returns a pointer to the target on success.

    NULL if no tool with the given name could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMBGEN_TARGET Target;

    CurrentEntry = Context->ToolList.Next;
    while (CurrentEntry != &(Context->ToolList)) {
        Target = LIST_VALUE(CurrentEntry, MBGEN_TARGET, ListEntry);
        if (strcmp(Target->Label, Name) == 0) {
            return Target;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

VOID
MbgenDestroyTool (
    PMBGEN_TOOL Tool
    )

/*++

Routine Description:

    This routine destroys a tool entry.

Arguments:

    Tool - Supplies a pointer to the tool to destroy.

Return Value:

    None.

--*/

{

    if (Tool->Name != NULL) {
        free(Tool->Name);
    }

    if (Tool->Command != NULL) {
        free(Tool->Command);
    }

    if (Tool->Description != NULL) {
        free(Tool->Description);
    }

    if (Tool->Depfile != NULL) {
        free(Tool->Depfile);
    }

    if (Tool->DepsFormat != NULL) {
        free(Tool->DepsFormat);
    }

    free(Tool);
    return;
}

VOID
MbgenPrintAllEntries (
    PMBGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine prints out all tools and targets.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    UINTN Index;
    PMBGEN_SCRIPT Script;
    PLIST_ENTRY ScriptEntry;
    PSTR ScriptRoot;
    PMBGEN_TARGET Target;
    PMBGEN_TOOL Tool;

    CurrentEntry = Context->ToolList.Next;
    while (CurrentEntry != &(Context->ToolList)) {
        Tool = LIST_VALUE(CurrentEntry, MBGEN_TOOL, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        printf("Tool: %s\n"
               "\tCommand: %s\n"
               "\tDescription: %s\n"
               "\tDepFile: %s\n"
               "\tDepsFormat: %s\n\n",
               Tool->Name,
               Tool->Command,
               Tool->Description,
               Tool->Depfile,
               Tool->DepsFormat);
    }

    ScriptEntry = Context->ScriptList.Next;
    while (ScriptEntry != &(Context->ScriptList)) {
        Script = LIST_VALUE(ScriptEntry, MBGEN_SCRIPT, ListEntry);
        ScriptEntry = ScriptEntry->Next;
        switch (Script->Root) {
        case MbgenSourceTree:
            ScriptRoot = "//";
            break;

        case MbgenBuildTree:
            ScriptRoot = "^/";
            break;

        case MbgenAbsolutePath:
            ScriptRoot = "/";
            break;

        default:

            assert(FALSE);

            ScriptRoot = "??";
            break;
        }

        printf("Script: %s%s (%d bytes, %d targets)\n",
               ScriptRoot,
               Script->Path,
               Script->Size,
               Script->TargetCount);

        CurrentEntry = Script->TargetList.Next;
        while (CurrentEntry != &(Script->TargetList)) {
            Target = LIST_VALUE(CurrentEntry, MBGEN_TARGET, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            printf("\tTarget: %s\n"
                   "\t\tOutput: %s\n"
                   "\t\tTool: %s\n"
                   "\t\tDeps: %d\n",
                   Target->Label,
                   Target->Output,
                   Target->Tool,
                   Target->Deps.Count);

            for (Index = 0; Index < Target->Deps.Count; Index += 1) {
                printf("\t\t\t%s:%s\n",
                       Target->Deps.List[Index]->Script->CompletePath,
                       Target->Deps.List[Index]->Label);
            }

            printf("\t\tPublicDeps: %d\n", Target->PublicDeps.Count);
            for (Index = 0; Index < Target->PublicDeps.Count; Index += 1) {
                printf("\t\t\t%s:%s\n",
                       Target->PublicDeps.List[Index]->Script->CompletePath,
                       Target->PublicDeps.List[Index]->Label);
            }

            if (Target->Config != NULL) {
                printf("\t\tConfig: ");
                ChalkPrintObject(Target->Config, 24);
                printf("\n");
            }

            if (Target->PublicConfig != NULL) {
                printf("\t\tPublicConfig: ");
                ChalkPrintObject(Target->PublicConfig, 24);
                printf("\n");
            }

            printf("\n");
        }
    }

    return;
}


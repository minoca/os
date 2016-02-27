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
    "      build files, rather than the default, build.mb.\n" \
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
MbgenAddInputsToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_INPUTS Inputs,
    PCHALK_OBJECT List
    );

INT
MbgenAddInputToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_INPUTS Inputs,
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

INT
MbgenAddInput (
    PMBGEN_INPUTS Inputs,
    PVOID Input
    );

VOID
MbgenDestroyInputs (
    PMBGEN_INPUTS Inputs
    );

VOID
MbgenDestroySource (
    PMBGEN_SOURCE Source
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
        "inputs",
        offsetof(MBGEN_TARGET, InputsObject),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "order-only-inputs",
        offsetof(MBGEN_TARGET, OrderOnlyInputsObject),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "callback",
        offsetof(MBGEN_TARGET, Callback),
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

    Status = MbgenCreateMakefile(&Context);

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
        ChalkPrintObject(stdout, List, 0);
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

    MbgenDestroyInputs(&(Target->Inputs));
    MbgenDestroyInputs(&(Target->OrderOnlyInputs));
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

    ULONG Advance;
    INT Status;
    PMBGEN_TARGET Target;

    Target = malloc(sizeof(MBGEN_TARGET));
    if (Target == NULL) {
        return ENOMEM;
    }

    memset(Target, 0, sizeof(MBGEN_TARGET));
    Target->Type = MbgenInputTarget;
    Target->Script = Script;
    Target->Tree = MbgenBuildTree;
    Status = ChalkConvertDictToStructure(&(Context->Interpreter),
                                         Entry,
                                         MbgenTargetMembers,
                                         Target);

    if (Status != 0) {
        goto ParseTargetEntryEnd;
    }

    //
    // At least one of the output or label must be specified.
    //

    if ((Target->Label == NULL) && (Target->Output == NULL)) {
        fprintf(stderr, "Error: label or output must be defined.\n");
        Status = EINVAL;
        goto ParseTargetEntryEnd;
    }

    if (Target->Label == NULL) {
        Target->Label = strdup(Target->Output);

    } else if (Target->Output == NULL) {
        Target->Output = strdup(Target->Label);
    }

    if ((Target->Label == NULL) || (Target->Output == NULL)) {
        Status = ENOMEM;
        goto ParseTargetEntryEnd;
    }

    //
    // Handle output tree specification.
    //

    Advance = 0;
    if (MBGEN_IS_SOURCE_ROOT_RELATIVE(Target->Output)) {
        Advance = 2;
        Target->Tree = MbgenSourceTree;

    } else if (MBGEN_IS_BUILD_ROOT_RELATIVE(Target->Output)) {
        Advance = 2;
        Target->Tree = MbgenBuildTree;

    } else if (*(Target->Output) == '/') {
        Advance = 1;
        Target->Tree = MbgenAbsolutePath;

    //
    // The default is the build tree, so the circumflex switches to the source
    // tree.
    //

    } else if (*(Target->Output) == '^') {
        Advance = 1;
        Target->Tree = MbgenSourceTree;
    }

    if (Advance != 0) {
        memmove(Target->Output,
                Target->Output + Advance,
                strlen(Target->Output) + 1 - Advance);
    }

    if (*(Target->Output) == '\0') {
        fprintf(stderr,
                "Error: Output must be non-empty.\n");

        Status = EINVAL;
        goto ParseTargetEntryEnd;
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
        goto ParseTargetEntryEnd;
    }

    //
    // The inputs must be lists.
    //

    if ((Target->InputsObject != NULL) &&
        (Target->InputsObject->Header.Type != ChalkObjectList)) {

        fprintf(stderr,
                "Error: inputs for %s:%s must be a list.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseTargetEntryEnd;
    }

    if ((Target->OrderOnlyInputsObject != NULL) &&
        (Target->OrderOnlyInputsObject->Header.Type != ChalkObjectList)) {

        fprintf(stderr,
                "Error: order-only-inputs for %s:%s must be a list.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseTargetEntryEnd;
    }

    //
    // The callback must be a function.
    //

    if ((Target->Callback != NULL) &&
        (Target->Callback->Header.Type != ChalkObjectFunction)) {

        fprintf(stderr,
                "Error: callback for %s:%s must be a function.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseTargetEntryEnd;
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
        goto ParseTargetEntryEnd;
    }

    INSERT_BEFORE(&(Target->ListEntry), &(Script->TargetList));
    Script->TargetCount += 1;
    Status = 0;

ParseTargetEntryEnd:
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
    // Convert the inputs to an array of input pointers to either sources or
    // other targets.
    //

    List = Target->InputsObject;
    if (List != NULL) {

        assert(Target->Inputs.Count == 0);

        Status = MbgenAddInputsToList(Context, Script, &(Target->Inputs), List);
        if (Status != 0) {
            goto ProcessTargetEnd;
        }
    }

    //
    // Load and find all the order-only inputs as well.
    //

    List = Target->OrderOnlyInputsObject;
    if (List != NULL) {

        assert(Target->OrderOnlyInputs.Count == 0);

        Status = MbgenAddInputsToList(Context,
                                      Script,
                                      &(Target->OrderOnlyInputs),
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
MbgenAddInputsToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_INPUTS Inputs,
    PCHALK_OBJECT List
    )

/*++

Routine Description:

    This routine adds the sources and targets described by the given list to
    the input list.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the current script.

    Inputs - Supplies a pointer to the inputs array to add to.

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
            goto AddInputsToListEnd;
        }

        Status = MbgenAddInputToList(Context,
                                     Script,
                                     Inputs,
                                     String->String.String);

        if (Status != 0) {
            fprintf(stderr,
                    "Error: %s: failed to add dependency %s: %s.\n",
                    Script->CompletePath,
                    String->String.String,
                    strerror(Status));

            goto AddInputsToListEnd;
        }
    }

    Status = 0;

AddInputsToListEnd:
    return Status;
}

INT
MbgenAddInputToList (
    PMBGEN_CONTEXT Context,
    PMBGEN_SCRIPT Script,
    PMBGEN_INPUTS Inputs,
    PSTR Name
    )

/*++

Routine Description:

    This routine adds the source or target described by the given name to the
    input list.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the current script.

    Inputs - Supplies a pointer to the inputs array to add to.

    Name - Supplies a pointer to the name of the target to add to the list.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    MBGEN_PATH Path;
    PMBGEN_SOURCE Source;
    INT Status;
    PMBGEN_TARGET Target;
    PMBGEN_SCRIPT TargetScript;

    Status = MbgenParsePath(Context,
                            Name,
                            MbgenSourceTree,
                            Script->Path,
                            &Path);

    if (Status != 0) {
        goto AddInputToListEnd;
    }

    if (Path.Target != NULL) {
        if (*Name == ':') {
            TargetScript = Script;

        } else {
            Status = MbgenLoadTargetScript(Context,
                                           &Path,
                                           MbgenScriptOrderTarget,
                                           &TargetScript);

            if (Status != 0) {
                goto AddInputToListEnd;
            }
        }
    }

    //
    // If there is no target name, it's a source.
    //

    if (Path.Target == NULL) {
        Source = malloc(sizeof(MBGEN_SOURCE));
        if (Source == NULL) {
            Status = ENOMEM;
            goto AddInputToListEnd;
        }

        memset(Source, 0, sizeof(MBGEN_SOURCE));
        Source->Type = MbgenInputSource;
        Source->Tree = Path.Root;
        Source->Path = Path.Path;
        Path.Path = NULL;
        Status = MbgenAddInput(Inputs, Source);
        if (Status != 0) {
            free(Source);
            goto AddInputToListEnd;
        }

    //
    // Add a target pointer as an input.
    //

    } else {

        //
        // Add all targets from the given script.
        //

        if (Path.Target[0] == '\0') {
            CurrentEntry = TargetScript->TargetList.Next;
            while (CurrentEntry != &(TargetScript->TargetList)) {
                Target = LIST_VALUE(CurrentEntry, MBGEN_TARGET, ListEntry);
                Status = MbgenAddInput(Inputs, Target);
                if (Status != 0) {
                    goto AddInputToListEnd;
                }

                CurrentEntry = CurrentEntry->Next;
            }

        //
        // Add the specified target.
        //

        } else {
            Target = MbgenFindTargetInScript(Context,
                                             TargetScript,
                                             Path.Target);

            if (Target == NULL) {
                fprintf(stderr,
                        "Error: Failed to find target %s:%s.\n",
                        TargetScript->CompletePath,
                        Path.Target);

                Status = ENOENT;
                goto AddInputToListEnd;
            }

            Status = MbgenAddInput(Inputs, Target);
            if (Status != 0) {
                goto AddInputToListEnd;
            }
        }
    }

    Status = 0;

AddInputToListEnd:
    if (Path.Path != NULL) {
        free(Path.Path);
    }

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

    CurrentEntry = Script->TargetList.Next;
    while (CurrentEntry != &(Script->TargetList)) {
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
    PMBGEN_TARGET InputTarget;
    PMBGEN_SCRIPT Script;
    PLIST_ENTRY ScriptEntry;
    PSTR ScriptPath;
    PSTR ScriptRoot;
    PMBGEN_SOURCE Source;
    PMBGEN_TARGET Target;
    PMBGEN_TOOL Tool;
    PSTR TreePath;

    CurrentEntry = Context->ToolList.Next;
    while (CurrentEntry != &(Context->ToolList)) {
        Tool = LIST_VALUE(CurrentEntry, MBGEN_TOOL, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        printf("Tool: %s\n"
               "\tCommand: %s\n"
               "\tDescription: %s\n",
               Tool->Name,
               Tool->Command,
               Tool->Description);

        if (Tool->Depfile != NULL) {
            printf("\tDepfile: %s\n", Tool->Depfile);
        }

        if (Tool->DepsFormat != NULL) {
            printf("\tDepsFormat: %s\n", Tool->DepsFormat);
        }

        printf("\n");
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

        ScriptPath = Script->Path;
        if (Script->Path == NULL) {
            ScriptPath = Context->ProjectFileName;
        }

        printf("Script: %s%s (%d bytes, %d targets)\n",
               ScriptRoot,
               ScriptPath,
               Script->Size,
               Script->TargetCount);

        CurrentEntry = Script->TargetList.Next;
        while (CurrentEntry != &(Script->TargetList)) {
            Target = LIST_VALUE(CurrentEntry, MBGEN_TARGET, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            TreePath = MbgenPathForTree(Context, Target->Tree);
            printf("\tTarget: %s\n\t\tOutput: %s/%s/%s\n",
                   Target->Label,
                   TreePath,
                   Target->Script->Path,
                   Target->Output);

            if (Target->Tool != NULL) {
                printf("\t\tTool %s\n", Target->Tool);
            }

            if (Target->Inputs.Count != 0) {
                printf("\t\tInputs: %d\n", Target->Inputs.Count);
                for (Index = 0; Index < Target->Inputs.Count; Index += 1) {
                    InputTarget = Target->Inputs.Array[Index];
                    switch (InputTarget->Type) {
                    case MbgenInputSource:
                        Source = (PMBGEN_SOURCE)InputTarget;
                        TreePath = MbgenPathForTree(Context, Source->Tree);
                        printf("\t\t\t%s%s\n", TreePath, Source->Path);
                        break;

                    case MbgenInputTarget:
                        TreePath = MbgenPathForTree(Context,
                                                    InputTarget->Script->Root);

                        printf("\t\t\t%s%s:%s\n",
                               TreePath,
                               InputTarget->Script->Path,
                               InputTarget->Label);

                        break;

                    default:

                        assert(FALSE);

                        break;
                    }
                }
            }

            if (Target->Config != NULL) {
                printf("\t\tConfig: ");
                ChalkPrintObject(stdout, Target->Config, 24);
                printf("\n");
            }

            printf("\n");
        }
    }

    return;
}

INT
MbgenAddInput (
    PMBGEN_INPUTS Inputs,
    PVOID Input
    )

/*++

Routine Description:

    This routine adds an input to the inputs list.

Arguments:

    Inputs - Supplies a pointer to the inputs array.

    Input - Supplies a pointer to the input to add.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PVOID NewBuffer;
    ULONG NewCapacity;

    if (Inputs->Count >= Inputs->Capacity) {
        NewCapacity = Inputs->Capacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = 16;
        }

        NewBuffer = realloc(Inputs->Array, NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        Inputs->Capacity = NewCapacity;
        Inputs->Array = NewBuffer;
    }

    assert(Inputs->Count < Inputs->Capacity);

    Inputs->Array[Inputs->Count] = Input;
    Inputs->Count += 1;
    return 0;
}

VOID
MbgenDestroyInputs (
    PMBGEN_INPUTS Inputs
    )

/*++

Routine Description:

    This routine destroys an inputs array, freeing all sources.

Arguments:

    Inputs - Supplies a pointer to the inputs array.

Return Value:

    None.

--*/

{

    ULONG Index;
    PMBGEN_SOURCE Source;

    for (Index = 0; Index < Inputs->Count; Index += 1) {
        Source = Inputs->Array[Index];
        if (Source->Type == MbgenInputSource) {
            MbgenDestroySource(Source);
        }
    }

    if (Inputs->Array != NULL) {
        free(Inputs->Array);
    }

    Inputs->Count = 0;
    Inputs->Capacity = 0;
    return;
}

VOID
MbgenDestroySource (
    PMBGEN_SOURCE Source
    )

/*++

Routine Description:

    This routine destroys a source entry.

Arguments:

    Source - Supplies a pointer to the source entry.

Return Value:

    None.

--*/

{

    if (Source->Path != NULL) {
        free(Source->Path);
    }

    free(Source);
    return;
}


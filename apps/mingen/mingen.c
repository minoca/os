/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    mingen.c

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
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mingen.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MINGEN_VERSION_MAJOR 1
#define MINGEN_VERSION_MINOR 0

#define MINGEN_USAGE                                                           \
    "usage: mingen [options] [targets...]\n"                                   \
    "The Minoca Build Generator creates Ninja files describing the build at \n"\
    "the current directory. If specific targets are specified, then a build \n"\
    "file for only those targets will be built. Otherwise, the build file \n"  \
    "is created for the whole project. Options are:\n"                         \
    "  -a, --args=expr -- Evaluate the given text in the script interpreter \n"\
    "      context before loading the project root file. This can be used \n"  \
    "      to pass configuration arguments and overrides to the build.\n"      \
    "      This can be specified multiple times.\n"                            \
    "  -D, --debug -- Print lots of information during execution.\n"           \
    "  -f, --format=fmt -- Specify the output format as make or ninja. The \n" \
    "      default is make.\n"                                                 \
    "  -g, --no-rebuild -- Don't include a re-generate rule in the output.\n"  \
    "  -n, --dry-run -- Do all the processing, but do not actually create \n"  \
    "      any output files.\n"                                                \
    "  -i, --input=project_file -- Use the given file as the top level \n"     \
    "      project file. The default is to search the current directory and \n"\
    "      parent directories for '.mgproj'.\n"                                \
    "  -o, --output=build_dir -- Set the given directory as the build \n"      \
    "      output directory.\n"                                                \
    "  -v, --verbose -- Print more information during processing.\n"           \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n\n"   \

#define MINGEN_OPTIONS_STRING "Df:ghi:no:vV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
MingenInitializeContext (
    PMINGEN_CONTEXT Context,
    INT ArgumentCount,
    PSTR *Arguments
    );

VOID
MingenDestroyContext (
    PMINGEN_CONTEXT Context
    );

INT
MingenParseToolEntry (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    );

INT
MingenParsePoolEntry (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    );

INT
MingenParseTargetEntry (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    );

INT
MingenProcessEntries (
    PMINGEN_CONTEXT Context
    );

INT
MingenProcessTool (
    PMINGEN_CONTEXT Context,
    PMINGEN_TOOL Tool
    );

INT
MingenProcessTarget (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target
    );

INT
MingenAddInputsToList (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target,
    PMINGEN_INPUTS Inputs,
    PCHALK_OBJECT List
    );

INT
MingenAddInputToList (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target,
    PMINGEN_INPUTS Inputs,
    PSTR Name
    );

INT
MingenMarkTargetNameActive (
    PMINGEN_CONTEXT Context,
    PSTR TargetName
    );

VOID
MingenMarkTargetActive (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target
    );

VOID
MingenMarkInputsActive (
    PMINGEN_CONTEXT Context,
    PMINGEN_INPUTS Inputs
    );

PMINGEN_TOOL
MingenFindTool (
    PMINGEN_CONTEXT Context,
    PSTR Name
    );

PMINGEN_POOL
MingenFindPool (
    PMINGEN_CONTEXT Context,
    PSTR Name
    );

PMINGEN_TARGET
MingenFindTargetInScript (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
    PSTR Name
    );

VOID
MingenDestroyTool (
    PMINGEN_TOOL Tool
    );

VOID
MingenDestroyPool (
    PMINGEN_POOL Pool
    );

VOID
MingenPrintAllEntries (
    PMINGEN_CONTEXT Context
    );

INT
MingenAddInput (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target,
    PMINGEN_INPUTS Inputs,
    PVOID Input
    );

VOID
MingenDestroyInputs (
    PMINGEN_INPUTS Inputs
    );

VOID
MingenDestroySource (
    PMINGEN_SOURCE Source
    );

//
// -------------------------------------------------------------------- Globals
//

struct option MingenLongOptions[] = {
    {"args", required_argument, 0, 'a'},
    {"debug", no_argument, 0, 'D'},
    {"format", required_argument, 0, 'f'},
    {"no-rebuild", no_argument, 0, 'g'},
    {"input", required_argument, 0, 'i'},
    {"dry-run", no_argument, 0, 'n'},
    {"output", required_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

CHALK_C_STRUCTURE_MEMBER MingenToolMembers[] = {
    {
        ChalkCString,
        "name",
        offsetof(MINGEN_TOOL, Name),
        TRUE,
        {0}
    },

    {
        ChalkCString,
        "command",
        offsetof(MINGEN_TOOL, Command),
        TRUE,
        {0}
    },

    {
        ChalkCString,
        "description",
        offsetof(MINGEN_TOOL, Description),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "depfile",
        offsetof(MINGEN_TOOL, Depfile),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "depsformat",
        offsetof(MINGEN_TOOL, DepsFormat),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "pool",
        offsetof(MINGEN_TOOL, Pool),
        FALSE,
        {0}
    },

    {0}
};

CHALK_C_STRUCTURE_MEMBER MingenPoolMembers[] = {
    {
        ChalkCString,
        "name",
        offsetof(MINGEN_POOL, Name),
        TRUE,
        {0}
    },

    {
        ChalkCInt32,
        "depth",
        offsetof(MINGEN_POOL, Depth),
        TRUE,
        {0}
    },

    {0}
};

CHALK_C_STRUCTURE_MEMBER MingenTargetMembers[] = {
    {
        ChalkCString,
        "label",
        offsetof(MINGEN_TARGET, Label),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "output",
        offsetof(MINGEN_TARGET, Output),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "inputs",
        offsetof(MINGEN_TARGET, InputsObject),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "implicit",
        offsetof(MINGEN_TARGET, ImplicitObject),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "orderonly",
        offsetof(MINGEN_TARGET, OrderOnlyObject),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "callback",
        offsetof(MINGEN_TARGET, Callback),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "tool",
        offsetof(MINGEN_TARGET, Tool),
        FALSE,
        {0}
    },

    {
        ChalkCString,
        "pool",
        offsetof(MINGEN_TARGET, Pool),
        FALSE,
        {0}
    },

    {
        ChalkCObjectPointer,
        "config",
        offsetof(MINGEN_TARGET, Config),
        FALSE,
        {0}
    },

    {
        ChalkCFlag32,
        "default",
        offsetof(MINGEN_TARGET, Flags),
        FALSE,
        {MINGEN_TARGET_DEFAULT}
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
    MINGEN_CONTEXT Context;
    INT Option;
    INT Status;

    srand(time(NULL) ^ getpid());
    Status = MingenInitializeContext(&Context, ArgumentCount, Arguments);
    if (Status != 0) {
        goto mainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MINGEN_OPTIONS_STRING,
                             MingenLongOptions,
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
                                           strlen(optarg),
                                           MingenScriptOrderCommandLine,
                                           NULL);

            if (Status == 0) {
                Status = ChalkExecuteDeferredScripts(
                                                  &(Context.Interpreter),
                                                  MingenScriptOrderCommandLine);
            }

            if (Status != 0) {
                fprintf(stderr,
                        "Error: Bad command line arguments script: %s\n",
                        optarg);

                goto mainEnd;
            }

            //
            // Save it so the command line can be recreated later.
            //

            Context.CommandScripts[Context.CommandScriptCount] = optarg;
            Context.CommandScriptCount += 1;
            break;

        case 'D':
            Context.Options |= MINGEN_OPTION_DEBUG;
            break;

        case 'f':
            if (strcasecmp(optarg, "make") == 0) {
                Context.Format = MingenOutputMake;

            } else if (strcasecmp(optarg, "ninja") == 0) {
                Context.Format = MingenOutputNinja;

            } else if (strcasecmp(optarg, "none") == 0) {
                Context.Format = MingenOutputNone;

            } else {
                fprintf(stderr,
                        "Error: Unknown output format %s. Valid values are "
                        "'make' and 'ninja'.\n",
                        optarg);

                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'g':
            Context.Options |= MINGEN_OPTION_NO_REBUILD_RULE;
            break;

        case 'i':
            Context.ProjectFilePath = strdup(optarg);
            if (Context.ProjectFilePath == NULL) {
                Status = ENOMEM;
                goto mainEnd;
            }

            break;

        case 'n':
            Context.Options |= MINGEN_OPTION_DRY_RUN;
            break;

        case 'o':
            Context.BuildRoot = MingenGetAbsoluteDirectory(optarg);
            if (Context.BuildRoot == NULL) {
                Status = errno;
                if (Status == 0) {
                    Status = -1;
                }

                fprintf(stderr,
                        "Error: Invalid build directory %s: %s\n",
                        optarg,
                        strerror(errno));

                goto mainEnd;
            }

            break;

        case 'v':
            Context.Options |= MINGEN_OPTION_VERBOSE;
            break;

        case 'V':
            printf("Minoca build generator version %d.%d\n"
                   "Copyright (c) 2015-2016 Minoca Corp. "
                   "All Rights Reserved.\n\n",
                   MINGEN_VERSION_MAJOR,
                   MINGEN_VERSION_MINOR);

            return 1;

        case 'h':
            printf(MINGEN_USAGE);
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
    if (ArgumentIndex < ArgumentCount) {
        Context.RequestedTargets = Arguments + ArgumentIndex;
        Context.RequestedTargetCount = ArgumentCount - ArgumentIndex;
    }

    if (Context.ProjectFilePath == NULL) {
        Status = MingenFindProjectFile(&Context);
        if (Status != 0) {
            goto mainEnd;
        }
    }

    //
    // Load the project root file. This also loads the default target file.
    //

    Status = MingenLoadProjectRoot(&Context);
    if (Status != 0) {
        fprintf(stderr, "Failed to load project root: %s.\n", strerror(Status));
        goto mainEnd;
    }

    //
    // Process the targets, which may cause more targets to get loaded.
    //

    Status = MingenProcessEntries(&Context);
    if (Status != 0) {
        goto mainEnd;
    }

    if ((Context.Options & MINGEN_OPTION_VERBOSE) != 0) {
        printf("Entries:\n");
        MingenPrintAllEntries(&Context);
        printf("\n");
    }

    switch (Context.Format) {
    case MingenOutputMake:
        Status = MingenCreateMakefile(&Context);
        if (Status != 0) {
            goto mainEnd;
        }

        if ((Context.Options & MINGEN_OPTION_VERBOSE) != 0) {
            printf("Creating build directories...");
        }

        //
        // Make won't automatically create the build directories needed like
        // Ninja, so go ahead and do that now.
        //

        Status = MingenCreateDirectories(&Context, &(Context.BuildDirectories));
        if (Status != 0) {
            fprintf(stderr,
                    "\nFailed to create build directories: %s.\n",
                    strerror(Status));

            goto mainEnd;
        }

        if ((Context.Options & MINGEN_OPTION_VERBOSE) != 0) {
            printf("done\n");
        }

        break;

    case MingenOutputNinja:
        Status = MingenCreateNinja(&Context);
        break;

    case MingenOutputNone:
    default:
        Status = 0;
        break;
    }

mainEnd:
    MingenDestroyContext(&Context);
    if (Status != 0) {
        fprintf(stderr,
                "mingen exiting with status %d: %s\n",
                Status,
                strerror(Status));
    }

    return Status;
}

VOID
MingenPrintRebuildCommand (
    PMINGEN_CONTEXT Context,
    FILE *File
    )

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

{

    PSTR Format;
    UINTN Index;

    switch (Context->Format) {
    case MingenOutputMake:
        Format = "make";
        break;

    case MingenOutputNinja:
        Format = "ninja";
        break;

    case MingenOutputNone:
        Format = "none";
        break;

    default:

        assert(FALSE);

        Format = "unknown";
        break;
    }

    fprintf(File,
            "%s --input=\"%s\" --output=\"%s\" --format=%s",
            Context->Executable,
            Context->ProjectFilePath,
            Context->BuildRoot,
            Format);

    for (Index = 0; Index < Context->CommandScriptCount; Index += 1) {
        fprintf(File, " --args='%s'", Context->CommandScripts[Index]);
    }

    for (Index = 0; Index < Context->RequestedTargetCount; Index += 1) {
        fprintf(File, " %s", Context->RequestedTargets[Index]);
    }

    return;
}

INT
MingenParseScriptResults (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script
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
    if ((Context->Options & MINGEN_OPTION_DEBUG) != 0) {
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

        Status = 0;
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

            Status = MingenParseTargetEntry(Context, Script, Entry);

        } else if (strcasecmp(Type->String.String, "tool") == 0) {
            Status = MingenParseToolEntry(Context, Script, Entry);

        } else if (strcasecmp(Type->String.String, "pool") == 0) {
            Status = MingenParsePoolEntry(Context, Script, Entry);

        } else if (strcasecmp(Type->String.String, "global_config") == 0) {
            Context->GlobalConfig = ChalkDictLookupCStringKey(Entry, "config");
            if ((Context->GlobalConfig != NULL) &&
                (Context->GlobalConfig->Header.Type != ChalkObjectDict)) {

                fprintf(stderr,
                        "Error: %s: global_config must be a dict.\n",
                        Script->CompletePath);

                Status = EINVAL;
                goto ParseScriptResultsEnd;
            }

        } else if (strcasecmp(Type->String.String, "ignore") != 0) {
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
MingenDestroyTarget (
    PMINGEN_TARGET Target
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

    MingenDestroyInputs(&(Target->Inputs));
    MingenDestroyInputs(&(Target->Implicit));
    MingenDestroyInputs(&(Target->OrderOnly));
    if (Target->Label != NULL) {
        free(Target->Label);
    }

    if (Target->Output != NULL) {
        free(Target->Output);
    }

    if (Target->Tool != NULL) {
        free(Target->Tool);
    }

    free(Target);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
MingenInitializeContext (
    PMINGEN_CONTEXT Context,
    INT ArgumentCount,
    PSTR *Arguments
    )

/*++

Routine Description:

    This routine initializes the mingen context.

Arguments:

    Context - Supplies a pointer to the context to initialize.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the arguments from the command line.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINTN AllocationSize;
    INT Status;

    if (ArgumentCount < 1) {
        return EINVAL;
    }

    memset(Context, 0, sizeof(MINGEN_CONTEXT));
    Context->Executable = Arguments[0];
    Context->Format = MingenOutputInvalid;
    INITIALIZE_LIST_HEAD(&(Context->ScriptList));
    INITIALIZE_LIST_HEAD(&(Context->ToolList));
    INITIALIZE_LIST_HEAD(&(Context->PoolList));
    INITIALIZE_LIST_HEAD(&(Context->SourceList));
    Status = ChalkInitializeInterpreter(&(Context->Interpreter));
    if (Status != 0) {
        return Status;
    }

    AllocationSize = (ArgumentCount - 1) * sizeof(PSTR);
    Context->CommandScripts = malloc(AllocationSize);
    if (Context->CommandScripts == NULL) {
        return ENOMEM;
    }

    memset(Context->CommandScripts, 0, AllocationSize);
    Status = 0;
    return Status;
}

VOID
MingenDestroyContext (
    PMINGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys a mingen context.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

{

    PMINGEN_POOL Pool;
    PMINGEN_SOURCE Source;
    PMINGEN_TOOL Tool;

    MingenDestroyAllScripts(Context);
    while (!LIST_EMPTY(&(Context->ToolList))) {
        Tool = LIST_VALUE(Context->ToolList.Next, MINGEN_TOOL, ListEntry);
        LIST_REMOVE(&(Tool->ListEntry));
        MingenDestroyTool(Tool);
    }

    while (!LIST_EMPTY(&(Context->PoolList))) {
        Pool = LIST_VALUE(Context->PoolList.Next, MINGEN_POOL, ListEntry);
        LIST_REMOVE(&(Pool->ListEntry));
        MingenDestroyPool(Pool);
    }

    while (!LIST_EMPTY(&(Context->SourceList))) {
        Source = LIST_VALUE(Context->SourceList.Next, MINGEN_SOURCE, ListEntry);

        assert(Source->Type == MingenInputSource);

        LIST_REMOVE(&(Source->ListEntry));
        MingenDestroySource(Source);
    }

    MingenDestroyPathList(&(Context->BuildDirectories));
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

    if (Context->FormatString != NULL) {
        free(Context->FormatString);
        Context->FormatString = NULL;
    }

    if (Context->BuildFileName != NULL) {
        free(Context->BuildFileName);
    }

    if (Context->ProjectFilePath != NULL) {
        free(Context->ProjectFilePath);
    }

    if (Context->CommandScripts != NULL) {
        free(Context->CommandScripts);
    }

    ChalkDestroyInterpreter(&(Context->Interpreter));
    return;
}

INT
MingenParseToolEntry (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
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
    PMINGEN_TOOL Tool;

    Tool = malloc(sizeof(MINGEN_TOOL));
    if (Tool == NULL) {
        return ENOMEM;
    }

    memset(Tool, 0, sizeof(MINGEN_TOOL));
    Status = ChalkConvertDictToStructure(&(Context->Interpreter),
                                         Entry,
                                         MingenToolMembers,
                                         Tool);

    if (Status != 0) {
        goto ParseToolEntryEnd;
    }

    if (MingenFindTool(Context, Tool->Name) != NULL) {
        fprintf(stderr, "Error: Duplicate tool %s.\n", Tool->Name);
        Status = EINVAL;
        goto ParseToolEntryEnd;
    }

    //
    // If no specific targets are requested, then all tools make it to the
    // output.
    //

    if (Context->RequestedTargetCount == 0) {
        Tool->Flags |= MINGEN_TOOL_ACTIVE;
    }

    INSERT_BEFORE(&(Tool->ListEntry), &(Context->ToolList));
    Status = 0;

ParseToolEntryEnd:
    if (Status != 0) {
        if (Tool != NULL) {
            MingenDestroyTool(Tool);
        }
    }

    return Status;
}

INT
MingenParsePoolEntry (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
    PCHALK_OBJECT Entry
    )

/*++

Routine Description:

    This routine parses a new pool entry.

Arguments:

    Context - Supplies a pointer to the context.

    Script - Supplies a pointer to the script being parsed.

    Entry - Supplies a pointer to the pool entry.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PMINGEN_POOL Pool;
    INT Status;

    Pool = malloc(sizeof(MINGEN_POOL));
    if (Pool == NULL) {
        return ENOMEM;
    }

    memset(Pool, 0, sizeof(MINGEN_POOL));
    Status = ChalkConvertDictToStructure(&(Context->Interpreter),
                                         Entry,
                                         MingenPoolMembers,
                                         Pool);

    if (Status != 0) {
        goto ParsePoolEntryEnd;
    }

    if (MingenFindPool(Context, Pool->Name) != NULL) {
        fprintf(stderr, "Error: Duplicate pool %s.\n", Pool->Name);
        Status = EINVAL;
        goto ParsePoolEntryEnd;
    }

    //
    // If no specific targets are requested, then all pools make it to the
    // output.
    //

    if (Context->RequestedTargetCount == 0) {
        Pool->Flags |= MINGEN_POOL_ACTIVE;
    }

    INSERT_BEFORE(&(Pool->ListEntry), &(Context->PoolList));
    Status = 0;

ParsePoolEntryEnd:
    if (Status != 0) {
        if (Pool != NULL) {
            MingenDestroyPool(Pool);
        }
    }

    return Status;
}

INT
MingenParseTargetEntry (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
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
    PSTR Output;
    BOOL Relative;
    INT Status;
    PMINGEN_TARGET Target;

    Target = malloc(sizeof(MINGEN_TARGET));
    if (Target == NULL) {
        return ENOMEM;
    }

    memset(Target, 0, sizeof(MINGEN_TARGET));
    Target->OriginalEntry = Entry;
    Target->Type = MingenInputTarget;
    Target->Script = Script;
    Target->Tree = MingenBuildTree;
    Status = ChalkConvertDictToStructure(&(Context->Interpreter),
                                         Entry,
                                         MingenTargetMembers,
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

    if (Target->Tool == NULL) {
        fprintf(stderr, "Error: %s missing tool\n", Target->Label);
        Status = EINVAL;
        goto ParseTargetEntryEnd;
    }

    //
    // Handle output tree specification.
    //

    Relative = TRUE;
    Advance = 0;
    if (MINGEN_IS_SOURCE_ROOT_RELATIVE(Target->Output)) {
        Advance = 2;
        Target->Tree = MingenSourceTree;
        Relative = FALSE;

    } else if (MINGEN_IS_BUILD_ROOT_RELATIVE(Target->Output)) {
        Advance = 2;
        Target->Tree = MingenBuildTree;
        Relative = FALSE;

    } else if (MINGEN_IS_ABSOLUTE_PATH(Target->Output)) {
        Advance = 0;
        Target->Tree = MingenAbsolutePath;
        Relative = FALSE;

    //
    // The default is the build tree, so the circumflex switches to the source
    // tree.
    //

    } else if (*(Target->Output) == '^') {
        Advance = 1;
        Target->Tree = MingenSourceTree;
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
    // Prepend the script path to the output if the output is relative.
    //

    if (Relative != FALSE) {
        Output = MingenAppendPaths(Script->Path, Target->Output);
        if (Output == NULL) {
            Status = ENOMEM;
            goto ParseTargetEntryEnd;
        }

        free(Target->Output);
        Target->Output = Output;
    }

    //
    // The label must be unique within the script.
    //

    if (MingenFindTargetInScript(Context, Script, Target->Label) != NULL) {
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

    if ((Target->ImplicitObject != NULL) &&
        (Target->ImplicitObject->Header.Type != ChalkObjectList)) {

        fprintf(stderr,
                "Error: implicit inputs for %s:%s must be a list.\n",
                Script->CompletePath,
                Target->Label);

        Status = EINVAL;
        goto ParseTargetEntryEnd;
    }

    if ((Target->OrderOnlyObject != NULL) &&
        (Target->OrderOnlyObject->Header.Type != ChalkObjectList)) {

        fprintf(stderr,
                "Error: order-only inputs for %s:%s must be a list.\n",
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
    // The config member must be a dictionary.
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

    //
    // If no specific targets are requested, then all targets make it to the
    // output.
    //

    if (Context->RequestedTargetCount == 0) {
        Target->Flags |= MINGEN_TARGET_ACTIVE;
    }

    INSERT_BEFORE(&(Target->ListEntry), &(Script->TargetList));
    Script->TargetCount += 1;
    Status = 0;

ParseTargetEntryEnd:
    if (Status != 0) {
        if (Target != NULL) {
            MingenDestroyTarget(Target);
        }
    }

    return Status;
}

INT
MingenProcessEntries (
    PMINGEN_CONTEXT Context
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

    ULONG Index;
    PMINGEN_SCRIPT Script;
    PLIST_ENTRY ScriptEntry;
    INT Status;
    PMINGEN_TARGET Target;
    PLIST_ENTRY TargetEntry;
    PSTR TargetName;
    PMINGEN_TOOL Tool;
    PLIST_ENTRY ToolEntry;

    Status = ENOENT;

    //
    // Iterate through all the scripts and all the targets in each script.
    // More scripts may get added onto the end of the list, but the list
    // iteration is safe since entries are never removed.
    //

    ScriptEntry = Context->ScriptList.Next;
    while (ScriptEntry != &(Context->ScriptList)) {
        Script = LIST_VALUE(ScriptEntry, MINGEN_SCRIPT, ListEntry);
        TargetEntry = Script->TargetList.Next;
        while (TargetEntry != &(Script->TargetList)) {
            Target = LIST_VALUE(TargetEntry, MINGEN_TARGET, ListEntry);
            Status = MingenProcessTarget(Context, Target);
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
        Tool = LIST_VALUE(ToolEntry, MINGEN_TOOL, ListEntry);
        Status = MingenProcessTool(Context, Tool);
        if (Status != 0) {
            fprintf(stderr, "Failed to process tool %s.\n", Tool->Name);
            goto ProcessEntriesEnd;
        }

        ToolEntry = ToolEntry->Next;
    }

    //
    // Deduplicate the build directory list.
    //

    MingenDeduplicatePathList(&(Context->BuildDirectories));

    //
    // If there are specifically requested targets, then follow the graph to
    // mark those as active.
    //

    for (Index = 0; Index < Context->RequestedTargetCount; Index += 1) {
        TargetName = Context->RequestedTargets[Index];
        Status = MingenMarkTargetNameActive(Context, TargetName);
        if (Status != 0) {
            goto ProcessEntriesEnd;
        }
    }

    Status = 0;

ProcessEntriesEnd:
    return Status;
}

INT
MingenProcessTool (
    PMINGEN_CONTEXT Context,
    PMINGEN_TOOL Tool
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
MingenProcessTarget (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target
    )

/*++

Routine Description:

    This routine processes a target entry, resolving all dependencies.

Arguments:

    Context - Supplies a pointer to the context.

    Target - Supplies a pointer to the target to process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR FileName;
    PCHALK_OBJECT List;
    MINGEN_PATH OutputPath;
    PMINGEN_PATH_LIST PathList;
    PSTR PathString;
    INT Status;

    //
    // Add the target file as a build directory, then split the path to make
    // it a directory.
    //

    if ((Target->Tool == NULL) || (strcmp(Target->Tool, "phony") != 0)) {
        OutputPath.Root = Target->Tree;
        OutputPath.Path = Target->Output;
        OutputPath.Target = NULL;
        Status = MingenAddPathToList(&(Context->BuildDirectories), &OutputPath);
        if (Status != 0) {
            goto ProcessTargetEnd;
        }

        PathList = &(Context->BuildDirectories);
        PathString = PathList->Array[PathList->Count - 1].Path;
        MingenSplitPath(PathString, NULL, &FileName);
        if (FileName == PathString) {
            free(PathString);
            PathList->Count -= 1;
        }
    }

    //
    // Convert the inputs to an array of input pointers to either sources or
    // other targets.
    //

    List = Target->InputsObject;
    if (List != NULL) {

        assert(Target->Inputs.Count == 0);

        Status = MingenAddInputsToList(Context,
                                       Target,
                                       &(Target->Inputs),
                                       List);

        if (Status != 0) {
            goto ProcessTargetEnd;
        }
    }

    //
    // Load and find all the implicit inputs as well.
    //

    List = Target->ImplicitObject;
    if (List != NULL) {

        assert(Target->Implicit.Count == 0);

        Status = MingenAddInputsToList(Context,
                                       Target,
                                       &(Target->Implicit),
                                       List);

        if (Status != 0) {
            goto ProcessTargetEnd;
        }
    }

    //
    // Load and find all the order-only inputs.
    //

    List = Target->OrderOnlyObject;
    if (List != NULL) {

        assert(Target->OrderOnly.Count == 0);

        Status = MingenAddInputsToList(Context,
                                       Target,
                                       &(Target->OrderOnly),
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
MingenAddInputsToList (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target,
    PMINGEN_INPUTS Inputs,
    PCHALK_OBJECT List
    )

/*++

Routine Description:

    This routine adds the sources and targets described by the given list to
    the input list.

Arguments:

    Context - Supplies a pointer to the context.

    Target - Supplies a pointer to the target the inputs are being added to.

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
                    Target->Script->CompletePath);

            Status = EINVAL;
            goto AddInputsToListEnd;
        }

        Status = MingenAddInputToList(Context,
                                      Target,
                                      Inputs,
                                      String->String.String);

        if (Status != 0) {
            fprintf(stderr,
                    "Error: %s: failed to add dependency %s: %s.\n",
                    Target->Script->CompletePath,
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
MingenAddInputToList (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target,
    PMINGEN_INPUTS Inputs,
    PSTR Name
    )

/*++

Routine Description:

    This routine adds the source or target described by the given name to the
    input list.

Arguments:

    Context - Supplies a pointer to the context.

    Target - Supplies a pointer to the target the inputs are being added to.

    Inputs - Supplies a pointer to the inputs array to add to.

    Name - Supplies a pointer to the name of the target to add to the list.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMINGEN_TARGET Dependency;
    PMINGEN_SCRIPT DependencyScript;
    MINGEN_PATH Path;
    PMINGEN_SOURCE Source;
    INT Status;

    DependencyScript = NULL;
    Status = MingenParsePath(Context,
                             Name,
                             MingenSourceTree,
                             Target->Script->Path,
                             &Path);

    if (Status != 0) {
        goto AddInputToListEnd;
    }

    if (Path.Target != NULL) {
        if (*Name == ':') {
            DependencyScript = Target->Script;

        } else {
            Status = MingenLoadTargetScript(Context,
                                            &Path,
                                            &DependencyScript);

            if (Status != 0) {
                goto AddInputToListEnd;
            }

            assert(DependencyScript != NULL);
        }
    }

    //
    // If there is no target name, it's a source.
    //

    if (Path.Target == NULL) {
        Source = malloc(sizeof(MINGEN_SOURCE));
        if (Source == NULL) {
            Status = ENOMEM;
            goto AddInputToListEnd;
        }

        memset(Source, 0, sizeof(MINGEN_SOURCE));
        Source->Type = MingenInputSource;
        Source->Tree = Path.Root;
        Source->Path = Path.Path;
        Status = MingenAddInput(Context, Target, Inputs, Source);
        if (Status != 0) {
            free(Source);
            goto AddInputToListEnd;
        }

        Path.Path = NULL;
        INSERT_BEFORE(&(Source->ListEntry), &(Context->SourceList));

    //
    // Add a target pointer as an input.
    //

    } else {

        //
        // Add all targets from the given script.
        //

        if (Path.Target[0] == '\0') {
            CurrentEntry = DependencyScript->TargetList.Next;
            while (CurrentEntry != &(DependencyScript->TargetList)) {
                Dependency = LIST_VALUE(CurrentEntry, MINGEN_TARGET, ListEntry);
                Status = MingenAddInput(Context, Target, Inputs, Dependency);
                if (Status != 0) {
                    goto AddInputToListEnd;
                }

                CurrentEntry = CurrentEntry->Next;
            }

        //
        // Add the specified target.
        //

        } else {
            Dependency = MingenFindTargetInScript(Context,
                                                  DependencyScript,
                                                  Path.Target);

            if (Dependency == NULL) {
                fprintf(stderr,
                        "Error: Failed to find target %s:%s.\n",
                        DependencyScript->CompletePath,
                        Path.Target);

                Status = ENOENT;
                goto AddInputToListEnd;
            }

            Status = MingenAddInput(Context, Target, Inputs, Dependency);
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

INT
MingenMarkTargetNameActive (
    PMINGEN_CONTEXT Context,
    PSTR TargetName
    )

/*++

Routine Description:

    This routine marks the given target name and all of its dependencies, tools,
    and pools as active.

Arguments:

    Context - Supplies a pointer to the context.

    TargetName - Supplies the name of the target.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    MINGEN_PATH Path;
    PMINGEN_SCRIPT Script;
    INT Status;
    PMINGEN_TARGET Target;

    Path.Path = NULL;
    Status = MingenParsePath(Context,
                             TargetName,
                             MingenSourceTree,
                             NULL,
                             &Path);

    if (Status != 0) {
        goto MarkTargetNameActiveEnd;
    }

    Script = MingenFindScript(Context, &Path);
    if (Script == NULL) {
        Status = ENOENT;
        goto MarkTargetNameActiveEnd;
    }

    //
    // If there's a specific target, mark that one as active and default.
    //

    if (Path.Target != NULL) {
        Target = MingenFindTargetInScript(Context, Script, Path.Target);
        if (Target == NULL) {
            Status = ENOENT;
            goto MarkTargetNameActiveEnd;
        }

        Target->Flags |= MINGEN_TARGET_DEFAULT;
        MingenMarkTargetActive(Context, Target);

    //
    // Mark all targets in the script as active.
    //

    } else {
        CurrentEntry = Script->TargetList.Next;
        while (CurrentEntry != &(Script->TargetList)) {
            Target = LIST_VALUE(CurrentEntry, MINGEN_TARGET, ListEntry);
            Target->Flags |= MINGEN_TARGET_DEFAULT;
            MingenMarkTargetActive(Context, Target);;
            CurrentEntry = CurrentEntry->Next;
        }
    }

    Status = 0;

MarkTargetNameActiveEnd:
    if (Status != 0) {
        fprintf(stderr,
                "Error: Failed to select requested target '%s': %s\n",
                TargetName,
                strerror(Status));
    }

    if (Path.Path != NULL) {
        free(Path.Path);
    }

    return Status;
}

VOID
MingenMarkTargetActive (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target
    )

/*++

Routine Description:

    This routine marks the given target, as well as all of its dependencies,
    tools, and pools active. This routine is recursive.

Arguments:

    Context - Supplies a pointer to the context.

    Target - Supplies a pointer to the target to mark active.

Return Value:

    None.

--*/

{

    PMINGEN_POOL Pool;
    PMINGEN_TOOL Tool;

    //
    // Avoid infinite recursion if there's somehow a loop in the graph.
    //

    if ((Target->Flags & MINGEN_TARGET_ACTIVE) != 0) {
        return;
    }

    Target->Flags |= MINGEN_TARGET_ACTIVE;
    Target->Script->Flags |= MINGEN_SCRIPT_ACTIVE;
    if (Target->Tool != NULL) {
        Tool = MingenFindTool(Context, Target->Tool);
        if (Tool != NULL) {
            Tool->Flags |= MINGEN_TOOL_ACTIVE;
        }
    }

    if (Target->Pool != NULL) {
        Pool = MingenFindPool(Context, Target->Pool);
        if (Pool != NULL) {
            Pool->Flags |= MINGEN_POOL_ACTIVE;
        }
    }

    MingenMarkInputsActive(Context, &(Target->Inputs));
    MingenMarkInputsActive(Context, &(Target->Implicit));
    MingenMarkInputsActive(Context, &(Target->OrderOnly));
    return;
}

VOID
MingenMarkInputsActive (
    PMINGEN_CONTEXT Context,
    PMINGEN_INPUTS Inputs
    )

/*++

Routine Description:

    This routine marks the given inputs, as well as all of their dependencies,
    tools, and pools active. This routine is recursive.

Arguments:

    Context - Supplies a pointer to the context.

    Inputs - Supplies a poitner to the input list to mark active.

Return Value:

    None.

--*/

{

    ULONG Index;
    PMINGEN_TARGET Input;

    for (Index = 0; Index < Inputs->Count; Index += 1) {
        Input = Inputs->Array[Index];
        if (Input->Type == MingenInputTarget) {
            MingenMarkTargetActive(Context, Input);
        }
    }

    return;
}

PMINGEN_TOOL
MingenFindTool (
    PMINGEN_CONTEXT Context,
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
    PMINGEN_TOOL Tool;

    CurrentEntry = Context->ToolList.Next;
    while (CurrentEntry != &(Context->ToolList)) {
        Tool = LIST_VALUE(CurrentEntry, MINGEN_TOOL, ListEntry);
        if (strcmp(Tool->Name, Name) == 0) {
            return Tool;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

PMINGEN_POOL
MingenFindPool (
    PMINGEN_CONTEXT Context,
    PSTR Name
    )

/*++

Routine Description:

    This routine attempts to find a pool with the given name.

Arguments:

    Context - Supplies a pointer to the context.

    Name - Supplies a pointer to the pool name to find.

Return Value:

    Returns a pointer to the pool on success.

    NULL if no pool with the given name could be found.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMINGEN_POOL Pool;

    CurrentEntry = Context->PoolList.Next;
    while (CurrentEntry != &(Context->PoolList)) {
        Pool = LIST_VALUE(CurrentEntry, MINGEN_POOL, ListEntry);
        if (strcmp(Pool->Name, Name) == 0) {
            return Pool;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

PMINGEN_TARGET
MingenFindTargetInScript (
    PMINGEN_CONTEXT Context,
    PMINGEN_SCRIPT Script,
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
    PMINGEN_TARGET Target;

    CurrentEntry = Script->TargetList.Next;
    while (CurrentEntry != &(Script->TargetList)) {
        Target = LIST_VALUE(CurrentEntry, MINGEN_TARGET, ListEntry);
        if (strcmp(Target->Label, Name) == 0) {
            return Target;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NULL;
}

VOID
MingenDestroyTool (
    PMINGEN_TOOL Tool
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
MingenDestroyPool (
    PMINGEN_POOL Pool
    )

/*++

Routine Description:

    This routine destroys a pool entry.

Arguments:

    Pool - Supplies a pointer to the pool to destroy.

Return Value:

    None.

--*/

{

    if (Pool->Name != NULL) {
        free(Pool->Name);
    }

    free(Pool);
    return;
}

VOID
MingenPrintAllEntries (
    PMINGEN_CONTEXT Context
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
    PMINGEN_TARGET InputTarget;
    PMINGEN_POOL Pool;
    PMINGEN_SCRIPT Script;
    PLIST_ENTRY ScriptEntry;
    PSTR ScriptPath;
    PSTR ScriptRoot;
    PMINGEN_SOURCE Source;
    PMINGEN_TARGET Target;
    PMINGEN_TOOL Tool;
    PSTR TreePath;

    CurrentEntry = Context->ToolList.Next;
    while (CurrentEntry != &(Context->ToolList)) {
        Tool = LIST_VALUE(CurrentEntry, MINGEN_TOOL, ListEntry);
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

    CurrentEntry = Context->PoolList.Next;
    while (CurrentEntry != &(Context->PoolList)) {
        Pool = LIST_VALUE(CurrentEntry, MINGEN_POOL, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        printf("Pool: %s, Depth: %d\n", Pool->Name, Pool->Depth);
    }

    ScriptEntry = Context->ScriptList.Next;
    while (ScriptEntry != &(Context->ScriptList)) {
        Script = LIST_VALUE(ScriptEntry, MINGEN_SCRIPT, ListEntry);
        ScriptEntry = ScriptEntry->Next;
        switch (Script->Root) {
        case MingenSourceTree:
            ScriptRoot = "//";
            break;

        case MingenBuildTree:
            ScriptRoot = "^/";
            break;

        case MingenAbsolutePath:
            ScriptRoot = "";
            break;

        default:

            assert(FALSE);

            ScriptRoot = "??";
            break;
        }

        ScriptPath = Script->Path;
        if (Script->Path == NULL) {
            ScriptPath = Context->ProjectFilePath;
            ScriptRoot = "";
        }

        printf("Script: %s%s (%d bytes, %d targets)\n",
               ScriptRoot,
               ScriptPath,
               Script->Size,
               Script->TargetCount);

        CurrentEntry = Script->TargetList.Next;
        while (CurrentEntry != &(Script->TargetList)) {
            Target = LIST_VALUE(CurrentEntry, MINGEN_TARGET, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            TreePath = MingenPathForTree(Context, Target->Tree);
            printf("\tTarget: %s\n\t\tOutput: %s/%s\n",
                   Target->Label,
                   TreePath,
                   Target->Output);

            if (Target->Tool != NULL) {
                printf("\t\tTool %s\n", Target->Tool);
            }

            if (Target->Inputs.Count != 0) {
                printf("\t\tInputs: %d\n", Target->Inputs.Count);
                for (Index = 0; Index < Target->Inputs.Count; Index += 1) {
                    InputTarget = Target->Inputs.Array[Index];
                    switch (InputTarget->Type) {
                    case MingenInputSource:
                        Source = (PMINGEN_SOURCE)InputTarget;
                        TreePath = MingenPathForTree(Context, Source->Tree);
                        printf("\t\t\t%s%s\n", TreePath, Source->Path);
                        break;

                    case MingenInputTarget:
                        TreePath = MingenPathForTree(Context,
                                                    InputTarget->Script->Root);

                        printf("\t\t\t%s/%s:%s\n",
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

            if ((Target->Config != NULL) &&
                (!LIST_EMPTY(&(Target->Config->Dict.EntryList)))) {

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
MingenAddInput (
    PMINGEN_CONTEXT Context,
    PMINGEN_TARGET Target,
    PMINGEN_INPUTS Inputs,
    PVOID Input
    )

/*++

Routine Description:

    This routine adds an input to the inputs list.

Arguments:

    Context - Supplies a pointer to the application context.

    Target - Supplies the target the input is being added to.

    Inputs - Supplies a pointer to the inputs array.

    Input - Supplies a pointer to the input to add.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PMINGEN_TARGET Dependency;
    PVOID NewBuffer;
    ULONG NewCapacity;
    INT Status;

    if (Inputs->Count >= Inputs->Capacity) {
        NewCapacity = Inputs->Capacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = 16;
        }

        NewBuffer = realloc(Inputs->Array, NewCapacity * sizeof(PVOID));
        if (NewBuffer == NULL) {
            Status = ENOMEM;
            goto AddInputEnd;
        }

        Inputs->Capacity = NewCapacity;
        Inputs->Array = NewBuffer;
    }

    assert(Inputs->Count < Inputs->Capacity);

    Inputs->Array[Inputs->Count] = Input;
    Inputs->Count += 1;

    //
    // If this is the inputs list or implicit list, the
    // input is a target, and there's a callback, call the callback.
    //

    Dependency = Input;
    if ((Inputs != &(Target->OrderOnly)) &&
        (Dependency->Type == MingenInputTarget) &&
        (Dependency->Callback != NULL) &&
        (Dependency->Callback->Header.Type != ChalkObjectNull)) {

        if ((Context->Options & MINGEN_OPTION_DEBUG) != 0) {
            printf("Calling callback of '%s' for '%s'...",
                   Dependency->Label,
                   Target->Label);
        }

        Status = ChalkCExecuteFunction(&(Context->Interpreter),
                                       Dependency->Callback,
                                       NULL,
                                       Target->OriginalEntry,
                                       NULL);

        if ((Context->Options & MINGEN_OPTION_DEBUG) != 0) {
            printf("Done, %s\n", strerror(Status));
        }

        if (Status != 0) {
            goto AddInputEnd;
        }
    }

    Status = 0;

AddInputEnd:
    return Status;
}

VOID
MingenDestroyInputs (
    PMINGEN_INPUTS Inputs
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

    if (Inputs->Array != NULL) {
        free(Inputs->Array);
    }

    Inputs->Count = 0;
    Inputs->Capacity = 0;
    return;
}

VOID
MingenDestroySource (
    PMINGEN_SOURCE Source
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


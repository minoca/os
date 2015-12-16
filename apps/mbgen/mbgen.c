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
                                           0,
                                           NULL);

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
    // Load the project root file.
    //

    Status = MbgenLoadProjectRoot(&Context);

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

    MbgenDestroyAllScripts(Context);
    if (Context->SourceRoot != NULL) {
        free(Context->SourceRoot);
        Context->SourceRoot = NULL;
    }

    if (Context->BuildRoot != NULL) {
        free(Context->BuildRoot);
        Context->BuildRoot = NULL;
    }

    ChalkDestroyInterpreter(&(Context->Interpreter));
    return;
}


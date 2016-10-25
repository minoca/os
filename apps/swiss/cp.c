/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cp.c

Abstract:

    This module implements the cp (copy) utility.

Author:

    Evan Green 3-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CP_VERSION_MAJOR 1
#define CP_VERSION_MINOR 0

#define CP_USAGE                                                               \
    "usage: cp [-fip] source_file target_file\n"                               \
    "       cp [-fip] source_file... target\n"                                 \
    "       cp -R [-H|-L|-P] [-fip] source_file... target\n\n"                 \
    "The cp utility copies one or more files or directories. Options are:\n"   \
    "  -f, --force -- If the file exists and cannot be truncated, attempt \n"  \
    "        to unlink it.\n"                                                  \
    "  -i, --interactive -- Prompt before overwriting any existing file.\n"    \
    "  -p, --preserve -- Preserve file permissions, owners, and access "       \
    "times.\n"                                                                 \
    "  -R, --recursive -- Recursively copy subdirectories of each operand.\n"  \
    "  -r -- Recursive, same as -R.\n"                                         \
    "  -H -- Follow symbolic links specified in operands only.\n"              \
    "  -L, --dereference -- Follow all symbolic links.\n"                      \
    "  -P, --no-dereference -- Do not follow symbolic links.\n"                \
    "  -v, --verbose -- Print files being copied.\n"                           \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define CP_OPTIONS_STRING "fipRrHLPv"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option CpLongOptions[] = {
    {"force", no_argument, 0, 'f'},
    {"interactive", no_argument, 0, 'i'},
    {"preserve", no_argument, 0, 'p'},
    {"recursive", no_argument, 0, 'R'},
    {"dereference", no_argument, 0, 'L'},
    {"no-dereference", no_argument, 0, 'P'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
CpMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the cp utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    PSTR Argument;
    ULONG ArgumentIndex;
    PSTR FirstSource;
    INT Option;
    ULONG Options;
    PSTR SourceBaseName;
    ULONG SourceCount;
    struct stat Stat;
    int Status;
    PSTR Target;
    BOOL TargetIsDirectory;
    int TotalStatus;

    FirstSource = NULL;
    SourceCount = 0;
    Target = NULL;
    Options = COPY_OPTION_FOLLOW_OPERAND_LINKS;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CP_OPTIONS_STRING,
                             CpLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'f':
            Options |= COPY_OPTION_UNLINK;
            Options &= ~COPY_OPTION_INTERACTIVE;
            break;

        case 'i':
            Options |= COPY_OPTION_INTERACTIVE;
            break;

        case 'p':
            Options |= COPY_OPTION_PRESERVE_PERMISSIONS;
            break;

        case 'R':
        case 'r':
            Options |= COPY_OPTION_RECURSIVE;
            break;

        case 'H':
            Options |= COPY_OPTION_FOLLOW_OPERAND_LINKS;
            Options &= ~COPY_OPTION_FOLLOW_LINKS;
            break;

        case 'L':
            Options |= COPY_OPTION_FOLLOW_LINKS;
            Options &= ~COPY_OPTION_FOLLOW_OPERAND_LINKS;
            break;

        case 'P':
            Options &= ~(COPY_OPTION_FOLLOW_OPERAND_LINKS |
                         COPY_OPTION_FOLLOW_LINKS);

            break;

        case 'v':
            Options |= COPY_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(CP_VERSION_MAJOR, CP_VERSION_MINOR);
            return 1;

        case 'h':
            printf(CP_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        FirstSource = Arguments[ArgumentIndex];
        Target = Arguments[ArgumentCount - 1];
    }

    SourceCount = ArgumentCount - ArgumentIndex;

    //
    // Fail if there were not enough arguments.
    //

    if ((Target == NULL) || (SourceCount <= 1)) {
        SwPrintError(0, NULL, "Argument expected. Try --help for usage");
        return 1;
    }

    SourceCount -= 1;

    //
    // Figure out if the target is a directory.
    //

    TargetIsDirectory = FALSE;
    Status = SwStat(Target, TRUE, &Stat);
    if (Status == 0) {
        if (S_ISDIR(Stat.st_mode)) {
            TargetIsDirectory = TRUE;
        }

    } else if (Status != ENOENT) {
        TotalStatus = Status;
        SwPrintError(TotalStatus, Target, "Failed to stat target");
        goto MainEnd;
    }

    //
    // If there are only two operands and the target is not a directory and the
    // source is not a directory, then just copy the source to the destination
    // directly.
    //

    if ((SourceCount == 1) && (TargetIsDirectory == FALSE)) {
        Status = SwStat(FirstSource, TRUE, &Stat);
        if (Status != 0) {
            SwPrintError(Status, FirstSource, "Cannot stat");
            goto MainEnd;
        }

        if (!S_ISDIR(Stat.st_mode)) {
            Status = SwCopy(Options, FirstSource, Target);
            goto MainEnd;
        }
    }

    //
    // If there's more than one source and the target is not a directory, that's
    // a problem.
    //

    if ((SourceCount > 1) && (TargetIsDirectory == FALSE)) {
        TotalStatus = ENOTDIR;
        SwPrintError(TotalStatus, Target, "Cannot copy to");
        goto MainEnd;
    }

    //
    // Loop through the arguments again and perform the moves.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;

        //
        // Skip the target.
        //

        if (Argument == Target) {
            continue;
        }

        //
        // Get the final name component of the source, because
        // "cp mydir/myfile mydir2" results in a destination file of
        // "mydir2/myfile", not "mydir2/mydir/myfile".
        //

        SourceBaseName = basename(Argument);
        if (SourceBaseName == NULL) {
            TotalStatus = ENOMEM;
            continue;
        }

        //
        // Create an appended version of the path if the target is a directory.
        //

        if (TargetIsDirectory != FALSE) {
            Status = SwAppendPath(Target,
                                  strlen(Target) + 1,
                                  SourceBaseName,
                                  strlen(SourceBaseName) + 1,
                                  &AppendedPath,
                                  &AppendedPathSize);

            if (Status == FALSE) {
                TotalStatus = EINVAL;
                continue;
            }

            Status = SwCopy(Options, Argument, AppendedPath);
            free(AppendedPath);

        } else {
            Status = SwCopy(Options, Argument, Target);
        }

        if (Status != 0) {
            TotalStatus = Status;
        }
    }

MainEnd:
    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//


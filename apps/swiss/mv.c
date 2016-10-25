/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mv.c

Abstract:

    This module implements the mv (move) file utility.

Author:

    Evan Green 2-Jul-2013

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
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MV_VERSION_MAJOR 1
#define MV_VERSION_MINOR 0

#define MV_USAGE                                                            \
    "usage: mv [-fiv] source... target\n\n"                                 \
    "The mv utility moves files and directories.\n\n"                       \
    "  -f, --force -- Skip all prompts.\n"                                  \
    "  -i, --interactive -- Interactive mode. Prompt for each file.\n"      \
    "  -v, --verbose -- Verbose, print each file being removed.\n"          \
    "  --help -- Display this help text.\n"                                 \
    "  --version -- Display version information and exit.\n\n"

#define MV_OPTIONS_STRING ":fiv"

//
// Define rm options.
//

//
// Set this option to disable all prompts.
//

#define MV_OPTION_FORCE 0x00000001

//
// Set this option to set prompts for all files.
//

#define MV_OPTION_INTERACTIVE 0x00000002

//
// Set this option to print each file that's deleted.
//

#define MV_OPTION_VERBOSE 0x00000004

//
// This internal option is set if standard in is a terminal device.
//

#define MV_OPTION_STDIN_IS_TERMINAL 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
MvMove (
    INT Options,
    PSTR Argument,
    PSTR Target
    );

//
// -------------------------------------------------------------------- Globals
//

struct option MvLongOptions[] = {
    {"force", no_argument, 0, 'f'},
    {"interactive", no_argument, 0, 'i'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
MvMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the mv utility.

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
    PSTR LinkDestination;
    INT Option;
    ULONG Options;
    PSTR Source;
    ULONG SourceCount;
    struct stat Stat;
    int Status;
    PSTR Target;
    BOOL TargetIsDirectory;
    int TotalStatus;

    LinkDestination = NULL;
    SourceCount = 0;
    Target = NULL;
    TotalStatus = 0;
    Options = 0;
    if (isatty(STDIN_FILENO)) {
        Options |= MV_OPTION_STDIN_IS_TERMINAL;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             MV_OPTIONS_STRING,
                             MvLongOptions,
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
            Options |= MV_OPTION_FORCE;
            Options &= ~MV_OPTION_INTERACTIVE;
            break;

        case 'i':
            Options |= MV_OPTION_INTERACTIVE;
            Options &= ~MV_OPTION_FORCE;
            break;

        case 'v':
            Options |= MV_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(MV_VERSION_MAJOR, MV_VERSION_MINOR);
            return 1;

        case 'h':
            printf(MV_USAGE);
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
    // Figure out if the target is a directory, or a link to a directory.
    //

    TargetIsDirectory = FALSE;
    Status = SwStat(Target, FALSE, &Stat);
    if (Status == 0) {
        if (S_ISLNK(Stat.st_mode)) {
            Status = SwReadLink(Target, &LinkDestination);
            if (Status == 0) {
                Target = LinkDestination;
                Status = SwStat(Target, FALSE, &Stat);
                if (Status == 0) {
                    if (S_ISDIR(Stat.st_mode)) {
                        TargetIsDirectory = TRUE;
                    }
                }
            }

        } else if (S_ISDIR(Stat.st_mode)) {
            TargetIsDirectory = TRUE;
        }

    } else if (errno != ENOENT) {
        TotalStatus = errno;
        SwPrintError(TotalStatus, Target, "Failed to stat target");
        goto MainEnd;
    }

    //
    // If there's more than one source and the target is not a directory, that's
    // a problem.
    //

    if ((SourceCount > 1) && (TargetIsDirectory == FALSE)) {
        TotalStatus = ENOTDIR;
        SwPrintError(TotalStatus, Target, "Cannot move to");
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
        // Create an appended version of the path if the target is a directory.
        //

        if (TargetIsDirectory != FALSE) {
            Source = basename(Argument);
            if (Source == NULL) {
                SwPrintError(errno, Argument, "Unable to get base name of");
                TotalStatus = 1;
                continue;
            }

            Status = SwAppendPath(Target,
                                  strlen(Target) + 1,
                                  Source,
                                  strlen(Source) + 1,
                                  &AppendedPath,
                                  &AppendedPathSize);

            if (Status == FALSE) {
                TotalStatus = EINVAL;
                continue;
            }

            Status = MvMove(Options, Argument, AppendedPath);
            free(AppendedPath);

        } else {
            Status = MvMove(Options, Argument, Target);
        }

        if (Status != 0) {
            TotalStatus = Status;
        }
    }

MainEnd:
    if (LinkDestination != NULL) {
        free(LinkDestination);
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
MvMove (
    INT Options,
    PSTR Argument,
    PSTR Target
    )

/*++

Routine Description:

    This routine is the workhorse behind the mv application. It moves a source
    to a destination.

Arguments:

    Options - Supplies the application options.

    Argument - Supplies the object to move.

    Target - Supplies the target to move it to.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    BOOL Answer;
    ULONG CopyOptions;
    PSTR QuotedArgument;
    PSTR QuotedTarget;
    int Result;
    struct stat Stat;
    struct stat TargetStat;
    int TargetStatResult;

    TargetStatResult = SwStat(Target, FALSE, &TargetStat);
    if ((TargetStatResult != 0) && (errno != ENOENT)) {
        Result = errno;
        SwPrintError(Result, Target, "Could not stat");
        goto MoveEnd;
    }

    //
    // If the destination exists, the force option is off, and either
    // 1) The permissions don't allow writing and stdin is a terminal, or
    // 2) The interactive option is enabled,
    //
    // then print a prompt.
    //

    if ((TargetStatResult == 0) && ((Options & MV_OPTION_FORCE) == 0) &&
        ((((TargetStat.st_mode & S_IWUSR) == 0) &&
          ((Options & MV_OPTION_STDIN_IS_TERMINAL) == 0)) ||
         ((Options & MV_OPTION_INTERACTIVE) != 0))) {

        QuotedArgument = SwQuoteArgument(Argument);
        fprintf(stderr, "mv: Overwrite file '%s'? ", QuotedArgument);
        if (QuotedArgument != Argument) {
            free(QuotedArgument);
        }

        Result = SwGetYesNoAnswer(&Answer);
        if ((Result != 0) || (Answer == FALSE)) {
            goto MoveEnd;
        }
    }

    //
    // In verbose mode, print out what's going on.
    //

    if ((Options & MV_OPTION_VERBOSE) != 0) {
        QuotedArgument = SwQuoteArgument(Argument);
        QuotedTarget = SwQuoteArgument(Target);
        printf("'%s' -> '%s'\n", QuotedArgument, QuotedTarget);
        if (QuotedArgument != Argument) {
            free(QuotedArgument);
        }

        if (QuotedTarget != Target) {
            free(QuotedTarget);
        }
    }

    //
    // First try out a rename, and happily exit if it worked.
    //

    Result = rename(Argument, Target);
    if (Result == 0) {
        goto MoveEnd;
    }

    Result = errno;
    if ((Result != EXDEV) &&
        ((Result != EEXIST) && ((Options & MV_OPTION_FORCE) == 0))) {

        SwPrintError(Result, Argument, "Could not move");
        goto MoveEnd;
    }

    //
    // Stat the source.
    //

    Result = SwStat(Argument, FALSE, &Stat);
    if (Result != 0) {
        Result = errno;
        SwPrintError(Result, Argument, "Could not stat");
        goto MoveEnd;
    }

    //
    // There's more work to be done if the destination exists.
    //

    if (TargetStatResult == 0) {

        //
        // If the destination is a directory and the source is not or vice
        // versa, fail.
        //

        if ((S_ISDIR(TargetStat.st_mode)) != (S_ISDIR(Stat.st_mode))) {
            Result = ENOTDIR;
            if (S_ISDIR(TargetStat.st_mode)) {
                Result = EISDIR;
            }

            SwPrintError(Result, Target, "Could not move to target");
            goto MoveEnd;
        }

        //
        // Try to remove the destination.
        //

        if (S_ISDIR(TargetStat.st_mode)) {
            Result = SwRemoveDirectory(Target);

        } else {
            Result = SwUnlink(Target);
        }

        if (Result != 0) {
            Result = errno;
            SwPrintError(Result, Target, "Could not remove target");
            goto MoveEnd;
        }

        //
        // Try rename one more time just for fun.
        //

        Result = rename(Argument, Target);
        if (Result == 0) {
            goto MoveEnd;
        }
    }

    //
    // Attempt to duplicate the file hierarchy, then delete the old file
    // hierarchy.
    //

    CopyOptions = COPY_OPTION_FOLLOW_OPERAND_LINKS |
                  COPY_OPTION_PRESERVE_PERMISSIONS |
                  COPY_OPTION_RECURSIVE;

    Result = SwCopy(CopyOptions, Argument, Target);
    if (Result != 0) {
        QuotedArgument = SwQuoteArgument(Argument);
        SwPrintError(Result, Target, "Failed to copy '%s' to", QuotedArgument);
        goto MoveEnd;
    }

    Result = SwDelete(DELETE_OPTION_FORCE | DELETE_OPTION_RECURSIVE, Argument);
    if (Result != 0) {
        SwPrintError(Result, Argument, "Failed to remove");
        goto MoveEnd;
    }

MoveEnd:
    return Result;
}


/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ln.c

Abstract:

    This module implements the ln (link) utility.

Author:

    Evan Green 22-Oct-2013

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

#define LN_VERSION_MAJOR 1
#define LN_VERSION_MINOR 0

#define LN_USAGE                                                               \
    "usage: ln [-fs] source_file target_file\n"                                \
    "       ln [-fs] source_file ... target_directory\n"                       \
    "The ln utility creates a symbolic or hard link to the given file or \n"   \
    "within the given target directory. Options are:\n"                        \
    "  -f, --force -- Remove existing destination files.\n"                    \
    "  -L, --logical -- Dereference targets that are symbolic links.\n"        \
    "  -n, --no-dereference -- Treat the destination as a normal file if it \n"\
    "      is a symbolic link to a directory.\n"                               \
    "  -s, --symbolic -- Create symbolic links instead of hard links.\n"       \
    "  -v, --verbose -- Print files being linked.\n"                           \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define LN_OPTIONS_STRING "fLnsv"

//
// Define ln options.
//

//
// Set this option to remove existing destination files.
//

#define LN_OPTION_FORCE 0x00000001

//
// Set this option to create symbolic links instead of hard ones.
//

#define LN_OPTION_SYMBOLIC 0x00000002

//
// Set this option to print links created.
//

#define LN_OPTION_VERBOSE 0x00000004

//
// Set this option to treat the destination as a normal file if it is a
// symbolic link to a directory.
//

#define LN_OPTION_NO_DEREFERENCE 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
LnLink (
    ULONG Options,
    PSTR Source,
    PSTR Destination
    );

//
// -------------------------------------------------------------------- Globals
//

struct option LnLongOptions[] = {
    {"force", no_argument, 0, 'f'},
    {"logical", no_argument, 0, 'L'},
    {"no-dereference", no_argument, 0, 'n'},
    {"symbolic", no_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
LnMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the ln (link) utility.

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
    BOOL FollowLink;
    INT Option;
    ULONG Options;
    PSTR SourceBaseName;
    ULONG SourceCount;
    struct stat Stat;
    int Status;
    PSTR Target;
    BOOL TargetIsDirectory;
    int TotalStatus;

    SourceCount = 0;
    Target = NULL;
    Options = 0;
    TotalStatus = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             LN_OPTIONS_STRING,
                             LnLongOptions,
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
            Options |= LN_OPTION_FORCE;
            break;

        case 'L':
            Options &= ~LN_OPTION_NO_DEREFERENCE;
            break;

        case 'n':
            Options |= LN_OPTION_NO_DEREFERENCE;
            break;

        case 's':
            Options |= LN_OPTION_SYMBOLIC;
            break;

        case 'v':
            Options |= LN_OPTION_VERBOSE;
            break;

        case 'V':
            SwPrintVersion(LN_VERSION_MAJOR, LN_VERSION_MINOR);
            return 1;

        case 'h':
            printf(LN_USAGE);
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
    // Figure out if the target is a directory.
    //

    FollowLink = TRUE;
    if ((Options & LN_OPTION_NO_DEREFERENCE) != 0) {
        FollowLink = FALSE;
    }

    TargetIsDirectory = FALSE;
    Status = SwStat(Target, FollowLink, &Stat);
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
    // It is an error to specify more than one operand and the final one is
    // not an existing directory.
    //

    if ((SourceCount > 1) && (TargetIsDirectory == FALSE)) {
        SwPrintError(0, Target, "Target is not a directory");
        Status = EINVAL;
        goto MainEnd;
    }

    //
    // If there are only two operands and the target is not a directory, then
    // just link the source and target.
    //

    if ((SourceCount == 1) && (TargetIsDirectory == FALSE)) {
        Status = LnLink(Options, Arguments[ArgumentIndex], Target);
        goto MainEnd;
    }

    assert(TargetIsDirectory != FALSE);

    //
    // Loop through the arguments again and perform the links.
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
        // "ln mydir/myfile mydir2" results in a destination file of
        // "mydir2/myfile", not "mydir2/mydir/myfile".
        //

        SourceBaseName = basename(Argument);
        if (SourceBaseName == NULL) {
            TotalStatus = ENOMEM;
            continue;
        }

        //
        // Create an appended version of the path.
        //

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

        Status = LnLink(Options, Argument, AppendedPath);
        free(AppendedPath);
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

INT
LnLink (
    ULONG Options,
    PSTR Source,
    PSTR Destination
    )

/*++

Routine Description:

    This routine creates a link (hard or symbolic) to the source path at the
    destination path.

Arguments:

    Options - Supplies link options. See LN_OPTION_* definitions.

    Source - Supplies a pointer to the source of the link.

    Destination - Supplies a pointer to the destination of the link.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR LinkDestination;
    struct stat Stat;
    INT Status;

    LinkDestination = NULL;

    //
    // Check to see if the destination exists.
    //

    Status = SwStat(Destination, FALSE, &Stat);
    if (Status == 0) {

        //
        // It exists. If force is on, then try to unlink it.
        //

        if ((Options & LN_OPTION_FORCE) != 0) {
            Status = SwUnlink(Destination);
            if (Status != 0) {
                Status = errno;
                SwPrintError(Status, Destination, "Unable to delete");
                goto LinkEnd;
            }

        //
        // Force is off, so just complain and exit.
        //

        } else {
            Status = EEXIST;
            SwPrintError(Status, Destination, "Cannot create link at");
            goto LinkEnd;
        }
    }

    //
    // Create a symbolic or hard link.
    //

    if ((Options & LN_OPTION_SYMBOLIC) != 0) {
        if (SwSymlinkSupported != 0) {
            Status = SwCreateSymbolicLink(Source, Destination);

        } else {
            Status = SwCopy(0, Source, Destination);
        }

        if (Status != 0) {
            SwPrintError(Status, Destination, "Unable to link");
            goto LinkEnd;
        }

    //
    // Create a hard link.
    //

    } else {

        //
        // If the source is a symbolic link, use its destination.
        //

        Status = SwStat(Source, FALSE, &Stat);
        if (Status != 0) {
            SwPrintError(Status, Source, "Unable to stat");
            goto LinkEnd;
        }

        if (S_ISLNK(Stat.st_mode)) {
            Status = SwReadLink(Source, &LinkDestination);
            if (Status != 0) {
                SwPrintError(Status, Source, "Unable to read symbolic link");
                goto LinkEnd;
            }

            Source = LinkDestination;
        }

        Status = SwCreateHardLink(Source, Destination);
        goto LinkEnd;
    }

    if ((Options & LN_OPTION_VERBOSE) != 0) {
        printf("'%s' => '%s'\n", Destination, Source);
    }

LinkEnd:
    if (LinkDestination != NULL) {
        free(LinkDestination);
    }

    return Status;
}


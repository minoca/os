/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chmod.c

Abstract:

    This module implements support for the chmod utility.

Author:

    Evan Green 16-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this option to recursively change permissions for any directory.
//

#define CHMOD_OPTION_RECURSIVE 0x00000001

//
// Set this option to print out a message for each file changed.
//

#define CHMOD_OPTION_VERBOSE 0x00000002

//
// Set this option to suppress most error messages.
//

#define CHMOD_OPTION_QUIET 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ChmodChangePermissions (
    ULONG Options,
    PSTR ModeString,
    PSTR Argument
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ChmodMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the chmod utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    BOOL DidSomething;
    PSTR ModeString;
    ULONG Options;
    mode_t OriginalUmask;
    INT Result;
    INT ReturnValue;
    BOOL SkipControlArguments;

    ModeString = NULL;
    Options = 0;
    ReturnValue = 0;
    OriginalUmask = umask(0);

    //
    // Loop through all the options.
    //

    SkipControlArguments = TRUE;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        if (SkipControlArguments != FALSE) {

            //
            // Stop if a -- is found.
            //

            if (strcmp(Argument, "--") == 0) {
                SkipControlArguments = FALSE;
                continue;
            }

            if (Argument[0] == '-') {

                //
                // Parse the argument, unless they're actually permissions.
                //

                if ((Argument[1] != 'r') && (Argument[1] != 'w') &&
                    (Argument[1] != 'x') && (Argument[1] != 's') &&
                    (Argument[1] != 'X') && (Argument[1] != 't') &&
                    (Argument[1] != 'u') && (Argument[1] != 'g') &&
                    (Argument[1] != 'o') && (Argument[1] != 'a')) {

                    Argument += 1;
                    while (*Argument != '\0') {
                        switch (*Argument) {
                        case 'R':
                            Options |= CHMOD_OPTION_RECURSIVE;
                            break;

                        case 'v':
                            Options |= CHMOD_OPTION_VERBOSE;
                            Options &= ~CHMOD_OPTION_QUIET;
                            break;

                        case 'f':
                            Options |= CHMOD_OPTION_QUIET;
                            Options &= ~CHMOD_OPTION_VERBOSE;
                            break;

                        default:
                            SwPrintError(0,
                                         NULL,
                                         "Unknown option %c",
                                         *Argument);

                            Result = EINVAL;
                            goto MainEnd;
                        }

                        Argument += 1;
                    }

                    continue;
                }
            }
        }

        //
        // This is a regular argument.
        //

        if (ModeString == NULL) {
            ModeString = Argument;
        }
    }

    if (ModeString == NULL) {
        SwPrintError(0, NULL, "Expecting mode argument");
        ReturnValue = EINVAL;
        goto MainEnd;
    }

    //
    // Now that the options have been figured out, loop through again to
    // actually change permissions.
    //

    DidSomething = FALSE;
    SkipControlArguments = TRUE;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        if (SkipControlArguments != FALSE) {

            //
            // If a -- is found, all other arguments are real parameters.
            //

            if (strcmp(Argument, "--") == 0) {
                SkipControlArguments = FALSE;
                continue;
            }

            if (Argument[0] == '-') {
                continue;
            }
        }

        if (Argument == ModeString) {
            continue;
        }

        DidSomething = TRUE;
        Result = ChmodChangePermissions(Options, ModeString, Argument);
        if (Result != 0) {
            ReturnValue = Result;
        }
    }

    if (DidSomething == FALSE) {
        SwPrintError(0, NULL, "Argument expected");
        Result = 1;
        goto MainEnd;
    }

MainEnd:
    umask(OriginalUmask);
    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ChmodChangePermissions (
    ULONG Options,
    PSTR ModeString,
    PSTR Argument
    )

/*++

Routine Description:

    This routine changes the mode bits for the given file entry.

Arguments:

    Options - Supplies the invocation options. See CHMOD_OPTION_* definitions.

    ModeString - Supplies the string of mode bits to change.

    Argument - Supplies the path of the file to change.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    DIR *Directory;
    struct dirent *Entry;
    BOOL IsDirectory;
    mode_t NewMode;
    mode_t OriginalMode;
    CHAR PrintMode[10];
    PSTR QuotedPath;
    INT Result;
    struct stat Stat;
    INT StringIndex;
    PSTR Verb;

    Directory = NULL;

    //
    // Get the file information.
    //

    Result = SwStat(Argument, FALSE, &Stat);
    if (Result != 0) {
        Result = errno;
        if ((Options & CHMOD_OPTION_QUIET) == 0) {
            SwPrintError(Result, Argument, "Cannot stat");
        }

        goto ChangePermissionsEnd;
    }

    //
    // Skip symbolic links.
    //

    if (S_ISLNK(Stat.st_mode)) {
        if ((Options & CHMOD_OPTION_VERBOSE) != 0) {
            QuotedPath = SwQuoteArgument(Argument);
            printf("Neither symbolic link '%s' nor referent has been "
                   "changed.\n",
                   QuotedPath);

            if (QuotedPath != Argument) {
                free(QuotedPath);
            }
        }

        goto ChangePermissionsEnd;
    }

    OriginalMode = Stat.st_mode;
    NewMode = OriginalMode;
    IsDirectory = FALSE;
    if (S_ISDIR(Stat.st_mode)) {
        IsDirectory = TRUE;
    }

    Result = SwParseFilePermissionsString(ModeString, IsDirectory, &NewMode);
    if (Result == FALSE) {
        SwPrintError(0, ModeString, "Invalid mode");
        Result = EINVAL;
        goto ChangePermissionsEnd;
    }

    //
    // Attempt to change the mode of this file or directory.
    //

    Result = chmod(Argument, NewMode);
    if (Result != 0) {
        Result = errno;
        if ((Options & CHMOD_OPTION_QUIET) == 0) {
            SwPrintError(Result, Argument, "Could not change mode of");
        }

        goto ChangePermissionsEnd;
    }

    //
    // Print this out if verbose.
    //

    if ((Options & CHMOD_OPTION_VERBOSE) != 0) {
        for (StringIndex = 0; StringIndex < 9; StringIndex += 1) {
            PrintMode[StringIndex] = '-';
        }

        if ((NewMode & S_IRUSR) != 0) {
            PrintMode[0] = 'r';
        }

        if ((NewMode & S_IWUSR) != 0) {
            PrintMode[1] = 'w';
        }

        if ((NewMode & S_IXUSR) != 0) {
            PrintMode[2] = 'x';
            if ((NewMode & S_ISUID) != 0) {
                PrintMode[2] = 's';
            }

        } else {
            if ((NewMode & S_ISUID) != 0) {
                PrintMode[2] = 'S';
            }
        }

        if ((NewMode & S_IRGRP) != 0) {
            PrintMode[3] = 'r';
        }

        if ((NewMode & S_IWGRP) != 0) {
            PrintMode[4] = 'w';
        }

        if ((NewMode & S_IXGRP) != 0) {
            PrintMode[5] = 'x';
            if ((NewMode & S_ISGID) != 0) {
                PrintMode[5] = 's';
            }

        } else {
            if ((NewMode & S_ISGID) != 0) {
                PrintMode[5] = 'S';
            }
        }

        if ((NewMode & S_IROTH) != 0) {
            PrintMode[6] = 'r';
        }

        if ((NewMode & S_IWOTH) != 0) {
            PrintMode[7] = 'w';
        }

        if ((NewMode & S_IXOTH) != 0) {
            PrintMode[8] = 'x';
            if ((NewMode & S_ISVTX) != 0) {
                PrintMode[8] = 't';
            }

        } else {
            if ((NewMode & S_ISVTX) != 0) {
                PrintMode[8] = 'T';
            }
        }

        PrintMode[9] = '\0';
        if (NewMode != OriginalMode) {
            Verb = "changed to";

        } else {
            Verb = "retained as";
        }

        QuotedPath = SwQuoteArgument(Argument);
        printf("mode of '%s' %s 0%03o (%s)\n",
               QuotedPath,
               Verb,
               NewMode & (S_IRWXU | S_IRWXG | S_IRWXO),
               PrintMode);

        if (QuotedPath != Argument) {
            free(QuotedPath);
        }
    }

    //
    // If the options are not recursive or this is not a directory, then that's
    // all there is to do.
    //

    if (((Options & CHMOD_OPTION_RECURSIVE) == 0) || (IsDirectory == FALSE)) {
        goto ChangePermissionsEnd;
    }

    Directory = opendir(Argument);
    if (Directory == NULL) {
        Result = errno;
        if ((Options & CHMOD_OPTION_QUIET) == 0) {
            SwPrintError(Result, Argument, "Cannot open directory");
        }

        goto ChangePermissionsEnd;
    }

    //
    // Loop through all entries in the directory.
    //

    Result = 0;
    while (TRUE) {
        errno = 0;
        Entry = readdir(Directory);
        if (Entry == NULL) {
            Result = errno;
            if (Result != 0) {
                if ((Options & CHMOD_OPTION_QUIET) == 0) {
                    SwPrintError(Result, Argument, "Unable to read directory");
                    goto ChangePermissionsEnd;
                }
            }

            break;
        }

        if ((strcmp(Entry->d_name, ".") == 0) ||
            (strcmp(Entry->d_name, "..") == 0)) {

            continue;
        }

        Result = SwAppendPath(Argument,
                              strlen(Argument) + 1,
                              Entry->d_name,
                              strlen(Entry->d_name) + 1,
                              &AppendedPath,
                              &AppendedPathSize);

        if (Result == FALSE) {
            Result = ENOMEM;
            goto ChangePermissionsEnd;
        }

        Result = ChmodChangePermissions(Options, ModeString, AppendedPath);
        free(AppendedPath);
        if (Result != 0) {
            goto ChangePermissionsEnd;
        }
    }

ChangePermissionsEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    return Result;
}


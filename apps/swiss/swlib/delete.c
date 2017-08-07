/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    delete.c

Abstract:

    This module implements file deletion functionality for the Swiss common
    library.

Author:

    Evan Green 3-Jul-2013

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
#include <unistd.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define both read and write permissions globally.
//

#define DELETE_WRITABLE_PERMISSIONS \
    (S_IWUSR | S_IWGRP | S_IWOTH)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SwDelete (
    INT Options,
    PSTR Argument
    )

/*++

Routine Description:

    This routine is the workhorse behind the rm application. It removes one
    file or directory.

Arguments:

    Options - Supplies the application options.

    Argument - Supplies the object to remove.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    BOOL Answer;
    PSTR AppendedPath;
    ULONG AppendedPathSize;
    ULONG ArgumentLength;
    DIR *Directory;
    BOOL DirectoryEmpty;
    struct dirent *DirectoryEntry;
    BOOL InvalidArgument;
    PSTR QuotedArgument;
    INT Result;
    struct stat Stat;
    PSTR TypeString;
    PSTR WriteProtectedString;

    AppendedPath = NULL;
    Directory = NULL;

    //
    // If . or .. were the final components of this path, then print an error
    // message and skip them.
    //

    ArgumentLength = strlen(Argument);
    if (ArgumentLength == 0) {
        Result = EINVAL;
        goto RemoveEnd;
    }

    InvalidArgument = FALSE;
    if (Argument[ArgumentLength - 1] == '.') {
        if ((ArgumentLength == 1) ||
            (Argument[ArgumentLength - 2] == '/')) {

            InvalidArgument = TRUE;

        } else if (Argument[ArgumentLength - 2] == '.') {
            if ((ArgumentLength == 2) ||
                (Argument[ArgumentLength - 3] == '/')) {

                InvalidArgument = TRUE;
            }
        }
    }

    if (InvalidArgument != FALSE) {
        SwPrintError(0, Argument, "Cannot remove");
        Result = EINVAL;
        goto RemoveEnd;
    }

    //
    // Get some information about this file.
    //

    Result = SwStat(Argument, FALSE, &Stat);
    if (Result != 0) {
        Result = errno;
        if (((Options & DELETE_OPTION_FORCE) == 0) || (Result != ENOENT)) {
            SwPrintError(Result, Argument, "Cannot remove");

        } else {
            Result = 0;
        }

        goto RemoveEnd;
    }

    WriteProtectedString = "";
    if ((!S_ISLNK(Stat.st_mode)) &&
        (SwEvaluateFileTest(FileTestCanWrite, Argument, NULL) == FALSE)) {

        WriteProtectedString = "write protected ";

        //
        // If it's write protected and force is enabled, try to enable writing.
        //

        if ((Options & DELETE_OPTION_FORCE) != 0) {
            chmod(Argument, Stat.st_mode | DELETE_WRITABLE_PERMISSIONS);
            if (SwEvaluateFileTest(FileTestCanWrite, Argument, NULL) != FALSE) {
                WriteProtectedString = "";
            }
        }
    }

    TypeString = "entry";
    if (S_ISBLK(Stat.st_mode)) {
        TypeString = "block device";

    } else if (S_ISCHR(Stat.st_mode)) {
        TypeString = "character device";

    } else if (S_ISDIR(Stat.st_mode)) {
        TypeString = "directory";

    } else if (S_ISFIFO(Stat.st_mode)) {
        TypeString = "pipe";

    } else if (S_ISREG(Stat.st_mode)) {
        TypeString = "regular file";

    } else if (S_ISLNK(Stat.st_mode)) {
        TypeString = "link";

    } else if (S_ISSOCK(Stat.st_mode)) {
        TypeString = "socket";
    }

    //
    // Things get more interesting for directories.
    //

    if (S_ISDIR(Stat.st_mode)) {
        if ((Options & DELETE_OPTION_RECURSIVE) == 0) {
            Result = EISDIR;
            SwPrintError(Result, Argument, "Cannot remove");
            goto RemoveEnd;
        }

        //
        // Open up the directory to find out if there's anything in it.
        //

        Directory = opendir(Argument);
        if (Directory == NULL) {
            Result = errno;
            SwPrintError(Result, Argument, "Cannot open directory");
            goto RemoveEnd;
        }

        //
        // Determine if the directory is empty.
        //

        DirectoryEmpty = TRUE;
        while (TRUE) {
            errno = 0;
            DirectoryEntry = readdir(Directory);
            if (DirectoryEntry == NULL) {
                Result = errno;
                if (Result != 0) {
                    SwPrintError(Result, Argument, "Cannot read directory");
                    goto RemoveEnd;
                }

                break;
            }

            if ((strcmp(DirectoryEntry->d_name, ".") == 0) ||
                (strcmp(DirectoryEntry->d_name, "..") == 0)) {

                continue;
            }

            DirectoryEmpty = FALSE;
            break;
        }

        //
        // If the directory is not empty and it's interactive mode, then
        // ask about descending into the directory.
        //

        if ((DirectoryEmpty == FALSE) &&
            ((Options & DELETE_OPTION_INTERACTIVE) != 0)) {

            QuotedArgument = SwQuoteArgument(Argument);
            fprintf(stderr,
                    "%s: Descend into directory '%s'? ",
                    SwGetCurrentApplicationName(),
                    QuotedArgument);

            if (QuotedArgument != Argument) {
                free(QuotedArgument);
            }

            Result = SwGetYesNoAnswer(&Answer);
            if ((Result != 0) || (Answer == FALSE)) {
                goto RemoveEnd;
            }
        }

        //
        // Loop through and recursively remove each entry in the directory.
        // The first entry is already primed.
        //

        while (DirectoryEntry != NULL) {
            if ((strcmp(DirectoryEntry->d_name, ".") != 0) &&
                (strcmp(DirectoryEntry->d_name, "..") != 0)) {

                Result = SwAppendPath(Argument,
                                      strlen(Argument) + 1,
                                      DirectoryEntry->d_name,
                                      strlen(DirectoryEntry->d_name) + 1,
                                      &AppendedPath,
                                      &AppendedPathSize);

                if (Result == FALSE) {
                    Result = ENOMEM;
                    goto RemoveEnd;
                }

                Result = SwDelete(Options, AppendedPath);
                free(AppendedPath);
                AppendedPath = NULL;
                if (Result != 0) {
                    goto RemoveEnd;
                }
            }

            //
            // Move on to the next directory entry.
            //

            errno = 0;
            DirectoryEntry = readdir(Directory);
            Result = errno;
            if ((DirectoryEntry == NULL) && (Result != 0)) {
                SwPrintError(Result, Argument, "Cannot read directory");
                goto RemoveEnd;
            }
        }

        //
        // Finally, remove this directory. Prompt if force is off and either
        // 1) It's not writable and standard in is a terminal device. Or
        // 2) Interactive mode is set.
        //

        if (((Options & DELETE_OPTION_FORCE) == 0) &&
            (((SwEvaluateFileTest(FileTestCanWrite, Argument, NULL) == FALSE) &&
              ((Options & DELETE_OPTION_STDIN_IS_TERMINAL) != 0)) ||
             ((Options & DELETE_OPTION_INTERACTIVE) != 0))) {

            QuotedArgument = SwQuoteArgument(Argument);
            fprintf(stderr,
                    "%s: Remove %s%s '%s'? ",
                    SwGetCurrentApplicationName(),
                    WriteProtectedString,
                    TypeString,
                    QuotedArgument);

            if (QuotedArgument != Argument) {
                free(QuotedArgument);
            }

            Result = SwGetYesNoAnswer(&Answer);
            if ((Result != 0) || (Answer == FALSE)) {
                goto RemoveEnd;
            }
        }

        //
        // Pull the trigger.
        //

        Result = SwRemoveDirectory(Argument);

    //
    // This is not a directory, it's a file of some kind.
    //

    } else {

        //
        // Like above, prompt if force is off and either
        // 1) It's not writable and standard in is a terminal device. Or
        // 2) Interactive mode is set.
        //

        if (((Options & DELETE_OPTION_FORCE) == 0) &&
            (((!S_ISLNK(Stat.st_mode)) &&
              (SwEvaluateFileTest(FileTestCanWrite, Argument, NULL) == FALSE) &&
              ((Options & DELETE_OPTION_STDIN_IS_TERMINAL) != 0)) ||
             ((Options & DELETE_OPTION_INTERACTIVE) != 0))) {

            QuotedArgument = SwQuoteArgument(Argument);
            fprintf(stderr,
                    "%s: Remove %s%s '%s'? ",
                    SwGetCurrentApplicationName(),
                    WriteProtectedString,
                    TypeString,
                    QuotedArgument);

            if (QuotedArgument != Argument) {
                free(QuotedArgument);
            }

            Result = SwGetYesNoAnswer(&Answer);
            if ((Result != 0) || (Answer == FALSE)) {
                goto RemoveEnd;
            }

            chmod(Argument, Stat.st_mode | DELETE_WRITABLE_PERMISSIONS);
        }

        //
        // Pull the trigger.
        //

        Result = SwUnlink(Argument);
    }

    //
    // Print out an error if either the rmdir or unlink operation failed.
    //

    if (Result != 0) {
        Result = errno;
        SwPrintError(Result,
                     Argument,
                     "Could not remove %s%s",
                     WriteProtectedString,
                     TypeString);

        goto RemoveEnd;
    }

    //
    // In verbose mode, print a nice message indicating the file was deleted.
    //

    if ((Options & DELETE_OPTION_VERBOSE) != 0) {
        QuotedArgument = SwQuoteArgument(Argument);
        printf("%s: Removed %s%s '%s'.\n",
               SwGetCurrentApplicationName(),
               WriteProtectedString,
               TypeString,
               QuotedArgument);

        if (QuotedArgument != Argument) {
            free(QuotedArgument);
        }
    }

RemoveEnd:
    if (AppendedPath != NULL) {
        free(AppendedPath);
    }

    if (Directory != NULL) {
        closedir(Directory);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pwdcmd.c

Abstract:

    This module implements support for the pwd (print working directory)
    utility, which is both a shell builtin and a separate utility.

Author:

    Evan Green 26-Aug-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define PWD_INITIAL_CAPACITY 256
#define PWD_ENVIRONMENT_VARIABLE "PWD"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
PwdGetLogicalPwd (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SwPwdCommand (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the pwd (print working directory)
    utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    0 on success.

    Non-zero error on failure.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    ULONG ArgumentSize;
    ULONG Capacity;
    ULONG CharacterIndex;
    PSTR FreeValue;
    PSTR NewBuffer;
    BOOL Physical;
    INT Result;
    PSTR Search;
    PSTR Value;

    FreeValue = NULL;
    Physical = FALSE;

    //
    // Parse the arguments.
    //

    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        ArgumentSize = strlen(Argument);
        if (Argument[0] != '-') {
            continue;
        }

        if (strcmp(Argument, "--") == 0) {
            break;
        }

        for (CharacterIndex = 1;
             CharacterIndex < ArgumentSize;
             CharacterIndex += 1) {

            switch (Argument[CharacterIndex]) {
            case 'L':
                Physical = FALSE;
                break;

            case 'P':
                Physical = TRUE;
                break;

            default:
                SwPrintError(0,
                             NULL,
                             "Invalid option -%c",
                             Argument[CharacterIndex]);

                return 1;
            }
        }
    }

    Result = 0;
    Value = NULL;
    if (Physical == FALSE) {
        Value = PwdGetLogicalPwd();
        if (Value != NULL) {
            goto PwdCommandEnd;
        }
    }

    //
    // Use the get current directory function in the C library.
    //

    Capacity = PWD_INITIAL_CAPACITY;
    FreeValue = malloc(Capacity);
    if (FreeValue == NULL) {
        Result = ENOMEM;
        goto PwdCommandEnd;
    }

    while (TRUE) {
        if (getcwd(FreeValue, Capacity) != FreeValue) {
            if (errno != ERANGE) {
                Result = errno;
                goto PwdCommandEnd;
            }

            Capacity *= 2;
            NewBuffer = realloc(FreeValue, Capacity);
            if (NewBuffer == NULL) {
                Result = ENOMEM;
                goto PwdCommandEnd;
            }

            FreeValue = NewBuffer;

        } else {

            //
            // Convert any backslashes to forward slashes.
            //

            Search = FreeValue;
            while (*Search != '\0') {
                if (*Search == '\\') {
                    *Search = '/';
                }

                Search += 1;
            }

            Value = FreeValue;
            break;
        }
    }

PwdCommandEnd:
    if (Value == NULL) {
        Value = ".";
    }

    puts(Value);
    if (FreeValue != NULL) {
        free(FreeValue);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
PwdGetLogicalPwd (
    VOID
    )

/*++

Routine Description:

    This routine returns the logical working directory, which may contain an
    absolute path with symlinks.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the logical working directory on
    success. This memory comes from the environment, so it should not be
    modified or freed by the caller.

    NULL if the PWD environment variable was not set or was invalid.

--*/

{

    struct stat ActualDirectory;
    PSTR Directory;
    INT Result;
    PSTR Search;
    struct stat SupposedDirectory;

    Directory = getenv(PWD_ENVIRONMENT_VARIABLE);
    if (Directory == NULL) {
        return NULL;
    }

    if (*Directory == '\0') {
        return NULL;
    }

    //
    // The path must be absolute. Allow /whatever and x:/whatever.
    //

    if (*Directory != '/') {
        if (!isalpha(*Directory)) {
            return NULL;
        }

        if (Directory[1] != ':') {
            return NULL;
        }
    }

    //
    // Disallow "/./" and "/../" components.
    //

    Search = Directory;
    while (*Search != '\0') {
        if ((*Search == '/') || (*Search == '\\')) {
            if (Search[1] == '.') {
                if ((Search[2] == '/') || (Search[2] == '\\')) {
                    return NULL;

                } else if (Search[2] == '.') {
                    if ((Search[3] == '/') || (Search[3] == '\\')) {
                        return NULL;
                    }
                }
            }
        }

        Search += 1;
    }

    //
    // Ensure that this supposed absolute path points to the same place.
    //

    Result = SwOsStat(Directory, TRUE, &SupposedDirectory);
    if (Result != 0) {
        return NULL;
    }

    Result = SwOsStat(".", TRUE, &ActualDirectory);
    if (Result != 0) {
        return NULL;
    }

    if ((SupposedDirectory.st_dev != ActualDirectory.st_dev) ||
        (SupposedDirectory.st_ino != ActualDirectory.st_ino)) {

        return NULL;
    }

    //
    // Don't trust zero, as Windows returns zero for everything.
    //

    if (ActualDirectory.st_ino == 0) {
        return NULL;
    }

    return Directory;
}


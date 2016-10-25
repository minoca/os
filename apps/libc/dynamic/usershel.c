/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usershel.c

Abstract:

    This module implements support for the getusershell family of functions.

Author:

    Evan Green 9-Mar-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <paths.h>
#include <stdio.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define USER_SHELLS_PATH _PATH_SHELLS

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

FILE *ClUserShellsFile = NULL;
char *ClUserShellsLine = NULL;
size_t ClUserShellsLineSize = 0;
UINTN ClUserShellsIndex = 0;

PSTR ClBuiltinUserShells[] = {
    "/bin/sh",
    "/bin/csh",
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
getusershell (
    void
    )

/*++

Routine Description:

    This routine returns the next permitted user shell in the database of
    valid shells. This opens the file if necessary. This routine is neither
    thread-safe nor reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to a string containing the next shell on success. This
    buffer may be overwritten by subsequent calls to getusershell.

    NULL on failure or end-of-database.

--*/

{

    ssize_t LineSize;
    char *Result;

    if (ClUserShellsFile == NULL) {
        ClUserShellsFile = fopen(USER_SHELLS_PATH, "r");
        if (ClUserShellsFile == NULL) {

            assert(ClUserShellsIndex <
                   (sizeof(ClBuiltinUserShells) /
                    sizeof(ClBuiltinUserShells[0])));

            //
            // If there is no user shells file, pretend /bin/sh and /bin/csh
            // are there.
            //

            Result = ClBuiltinUserShells[ClUserShellsIndex];
            if (Result != NULL) {
                ClUserShellsIndex += 1;
            }

            return Result;
        }
    }

    //
    // Loop trying to get a valid line.
    //

    while (TRUE) {
        LineSize = getline(&ClUserShellsLine,
                           &ClUserShellsLineSize,
                           ClUserShellsFile);

        if (LineSize < 0) {

            //
            // If the file was unreadable, return the builtin shells.
            //

            if (ferror(ClUserShellsFile) != 0) {

                assert(ClUserShellsIndex <
                       (sizeof(ClBuiltinUserShells) /
                        sizeof(ClBuiltinUserShells[0])));

                Result = ClBuiltinUserShells[ClUserShellsIndex];
                if (Result != NULL) {
                    ClUserShellsIndex += 1;
                }

                return Result;
            }

            return NULL;
        }

        while ((LineSize != 0) && (isspace(ClUserShellsLine[LineSize - 1]))) {
            LineSize -= 1;
        }

        Result = ClUserShellsLine;
        Result[LineSize] = '\0';

        //
        // Get past whitespace.
        //

        while (isspace(*Result)) {
            Result += 1;
        }

        //
        // Skip any commented lines.
        //

        if ((*Result == '\0') || (*Result == '#')) {
            continue;
        }

        break;
    }

    return Result;
}

LIBC_API
void
setusershell (
    void
    )

/*++

Routine Description:

    This routine rewinds the user shells database back to the beginning.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClUserShellsFile != NULL) {
        if (fseek(ClUserShellsFile, 0, SEEK_SET) != 0) {
            fclose(ClUserShellsFile);
            ClUserShellsFile = NULL;
        }
    }

    ClUserShellsIndex = 0;
    return;
}

LIBC_API
void
endusershell (
    void
    )

/*++

Routine Description:

    This routine closes the permitted user shells database.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (ClUserShellsFile != NULL) {
        fclose(ClUserShellsFile);
        ClUserShellsFile = NULL;
    }

    ClUserShellsIndex = 0;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


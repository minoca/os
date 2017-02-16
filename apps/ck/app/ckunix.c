/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ckunix.c

Abstract:

    This module implements support for the Chalk application on POSIX like OSes.

Author:

    Evan Green 15-Aug-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <minoca/lib/types.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ChalkAddSearchPath (
    PVOID Vm,
    PSTR Directory,
    PSTR ChalkDirectory
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
ChalkSetupModulePath (
    PVOID Vm,
    PSTR Script
    )

/*++

Routine Description:

    This routine adds the default library search paths. This routine assumes
    there are at least two slots on the stack and that the module path list has
    already been pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Script - Supplies a pointer to the script path.

Return Value:

    None.

--*/

{

    PSTR Copy;
    PSTR CurrentDirectory;
    PSTR Home;
    PSTR Next;
    PSTR Path;
    PSTR ScriptBase;

    Copy = NULL;
    if (Script != NULL) {
        Copy = strdup(Script);
        if (Copy == NULL) {
            return;
        }
    }

    //
    // If a script was supplied, add the directory of the script.
    //

    if (Script != NULL) {
        ScriptBase = dirname(Copy);
        if (ScriptBase != NULL) {
            ChalkAddSearchPath(Vm, ScriptBase, NULL);
        }

    //
    // In interactive mode, add the current working directory.
    //

    } else {
        CurrentDirectory = getcwd(NULL, 0);
        if (CurrentDirectory != NULL) {
            ChalkAddSearchPath(Vm, CurrentDirectory, NULL);
            free(CurrentDirectory);
        }
    }

    if (Copy != NULL) {
        free(Copy);
        Copy = NULL;
    }

    //
    // Add the special environment variable.
    //

    Path = getenv("CK_LIBRARY_PATH");
    if (Path != NULL) {
        Copy = strdup(Path);
        if (Copy == NULL) {
            return;
        }

        Path = Copy;
        while (Path != NULL) {
            Next = strchr(Path, ':');
            if (Next != NULL) {
                *Next = '\0';
                Next += 1;
            }

            ChalkAddSearchPath(Vm, Path, NULL);
            Path = Next;
        }

        free(Copy);

    } else {

        //
        // Add the current user's home directory.
        //

        Home = getenv("HOME");
        if (Home != NULL) {
            ChalkAddSearchPath(Vm, Home, ".chalk");
        }

        //
        // Add some standard paths.
        //

        ChalkAddSearchPath(Vm, "/usr/local/lib", "chalk");
        ChalkAddSearchPath(Vm, "/usr/lib", "chalk");
        ChalkAddSearchPath(Vm, "/lib", "chalk");
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


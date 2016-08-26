/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

    PSTR CurrentDirectory;
    PSTR Home;
    PSTR Path;
    PSTR ScriptBase;

    //
    // If a script was supplied, add the directory of the script.
    //

    if (Script != NULL) {
        ScriptBase = basename(Script);
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

    //
    // Add the special environment variable.
    //

    Path = getenv("CK_LIBRARY_PATH");
    if (Path != NULL) {
        ChalkAddSearchPath(Vm, Path, NULL, MajorVersion);

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


/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    ckwin32.c

Abstract:

    This module implements support for the Chalk application on Windows.

Author:

    Evan Green 15-Aug-2016

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _WIN32_IE 0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <shlwapi.h>

#include <assert.h>
#include <libgen.h>
#include <stdio.h>

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

    CHAR Directory[MAX_PATH];
    HRESULT Result;
    PSTR ScriptBase;

    //
    // If a script was supplied, add the directory of the script.
    //

    if (Script != NULL) {
        ScriptBase = basename(Script);
        ChalkAddSearchPath(Vm, ScriptBase, NULL);

    //
    // In interactive mode, add the current working directory.
    //

    } else {
        if (GetCurrentDirectory(MAX_PATH, Directory) != 0) {
            ChalkAddSearchPath(Vm, Directory, NULL);
        }
    }

    //
    // Add the special environment variable directory.
    //

    if (GetEnvironmentVariable("CK_LIBRARY_PATH", Directory, MAX_PATH) != 0) {
        ChalkAddSearchPath(Vm, Directory, NULL);

    } else {

        //
        // Add the current user's application data directory.
        //

        Result = SHGetFolderPath(NULL,
                                 CSIDL_APPDATA,
                                 NULL,
                                 SHGFP_TYPE_CURRENT,
                                 Directory);

        if (Result == S_OK) {
            ChalkAddSearchPath(Vm, Directory, "chalk/chalk");
        }

        //
        // Add the users's home directory, since it's a bit more accessible to
        // most people.
        //

        Result = SHGetFolderPath(NULL,
                                 CSIDL_PROFILE,
                                 NULL,
                                 SHGFP_TYPE_CURRENT,
                                 Directory);

        if (Result == S_OK) {
            ChalkAddSearchPath(Vm, Directory, ".chalk/chalk");
        }

        //
        // Add the "all users" application data directory.
        //

        Result = SHGetFolderPath(NULL,
                                 CSIDL_COMMON_APPDATA,
                                 NULL,
                                 SHGFP_TYPE_CURRENT,
                                 Directory);

        if (Result == S_OK) {
            ChalkAddSearchPath(Vm, Directory, "chalk/chalk");
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


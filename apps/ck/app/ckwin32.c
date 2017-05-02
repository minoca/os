/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include <shlobj.h>

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

extern PSTR CkAppExecName;

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
    CHAR Directory[MAX_PATH];
    PSTR DirName;
    PSTR ExecName;
    PSTR Next;
    PSTR Path;
    HRESULT Result;
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
        ChalkAddSearchPath(Vm, ScriptBase, NULL);

    //
    // In interactive mode, add the current working directory.
    //

    } else {
        if (GetCurrentDirectory(MAX_PATH, Directory) != 0) {
            ChalkAddSearchPath(Vm, Directory, NULL);
        }
    }

    if (Copy != NULL) {
        free(Copy);
        Copy = NULL;
    }

    //
    // Add the special environment variable directory.
    //

    if (GetEnvironmentVariable("CK_LIBRARY_PATH", Directory, MAX_PATH) != 0) {
        Copy = strdup(Directory);
        if (Copy == NULL) {
            return;
        }

        Path = Copy;
        while (Path != NULL) {
            Next = strchr(Path, ';');
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
        // Add the sysroot-like path relative to the executable.
        //

        if ((CkAppExecName != NULL) && (*CkAppExecName != '\0')) {
            ExecName = strdup(CkAppExecName);
            if (ExecName != NULL) {
                DirName = dirname(ExecName);
                if (DirName != NULL) {
                    ChalkAddSearchPath(Vm, DirName, "../lib/chalk");
                }

                free(ExecName);
            }
        }

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


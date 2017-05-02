/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    app.c

Abstract:

    This module implements the Chalk app module, which provides an interface
    to the outer application.

Author:

    Evan Green 19-Oct-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef _WIN32

#define CK_APP_SUFFIX ".exe"
#define CK_APP_PATH_SEPARATOR "\\"

#else

#define CK_APP_SUFFIX ""
#define CK_APP_PATH_SEPARATOR "/"

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpAppModuleInit (
    PCK_VM Vm
    );

VOID
CkpAppSetExecName (
    PCSTR Argument0
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the pointer to the app arguments. This must be set before the app
// module is imported.
//

INT CkAppArgc;
PSTR *CkAppArgv;

//
// Define the original application name.
//

PCSTR CkAppExecName = "";

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadAppModule (
    PCK_VM Vm,
    PCSTR Argument0
    )

/*++

Routine Description:

    This routine preloads the app module. It is called to make the presence of
    the module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Argument0 - Supplies the zeroth argument to the original command line. This
        is used to find the application executable.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    //
    // Set the original exec name.
    //

    CkpAppSetExecName(Argument0);
    return CkPreloadForeignModule(Vm, "app", NULL, NULL, CkpAppModuleInit);
}

VOID
CkpAppModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSTR Argument;
    INT Index;

    //
    // Create and populate a list for argv.
    //

    CkGetVariable(Vm, 0, "List");
    CkCall(Vm, 0);
    for (Index = 0; Index < CkAppArgc; Index += 1) {
        Argument = CkAppArgv[Index];
        CkPushValue(Vm, -1);
        CkPushString(Vm, Argument, strlen(Argument));
        CkCallMethod(Vm, "append", 1);
        CkStackPop(Vm);
    }

    CkSetVariable(Vm, 0, "argv");
    CkPushString(Vm, CkAppExecName, strlen(CkAppExecName));
    CkSetVariable(Vm, 0, "execName");
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpAppSetExecName (
    PCSTR Argument0
    )

/*++

Routine Description:

    This routine sets the absolute path to the application based on the zeroth
    argument.

Arguments:

    Argument0 - Supplies a pointer to the initial command line argument.

Return Value:

    None. The path is set in the exec name global on success.

--*/

{

    size_t Arg0Length;
    PSTR CurrentPath;
    PSTR Final;
    size_t Length;
    PSTR NextPath;
    CHAR Path[512];
    PSTR PathVariable;
    int PrintLength;
    CHAR Separator;
    PCSTR Slash;
    PCSTR Suffix;
    PSTR WorkingDirectory;

    Final = NULL;
    PathVariable = NULL;
    WorkingDirectory = NULL;

    //
    // If applications end in suffixes (like .exe), then see if argument 0 has
    // that. If it does, there's no need to append it.
    //

    Arg0Length = strlen(Argument0);
    Suffix = CK_APP_SUFFIX;
    if (*Suffix != '\0') {
        Length = strlen(Suffix);
        if ((Arg0Length > Length) &&
            (strcmp(Argument0 + Arg0Length - Length, Suffix) == 0)) {

            Suffix = "";
        }
    }

    //
    // If the path is already absolute, then use it.
    //

    if ((*Argument0 == '/') ||
        (isalpha(*Argument0) && (Argument0[1] == ':') &&
         ((Argument0[2] == '/') || (Argument0[2] == '\\')))) {

        PrintLength = snprintf(Path, sizeof(Path), "%s%s", Argument0, Suffix);
        if (PrintLength < sizeof(Path)) {
            Final = Path;
        }

        goto AppSetExecNameEnd;
    }

    //
    // If the path has a slash in it, then prepend the current directory and
    // use that.
    //

    Slash = strchr(Argument0, '/');
    if (Slash == NULL) {
        Slash = strchr(Argument0, '\\');
    }

    if (Slash != NULL) {

        //
        // Skip ./ for prettiness.
        //

        if ((Slash == Argument0 + 1) && (*Argument0 == '.')) {
            Argument0 += 2;
        }

        WorkingDirectory = getcwd(NULL, 0);
        if (WorkingDirectory != NULL) {
            PrintLength = snprintf(Path,
                                   sizeof(Path),
                                   "%s%c%s%s",
                                   WorkingDirectory,
                                   *Slash,
                                   Argument0,
                                   Suffix);

            if (PrintLength < sizeof(Path)) {
                Final = Path;
            }
        }

        goto AppSetExecNameEnd;
    }

    //
    // Okay, it's time to start looking through the PATH. Get and copy the
    // variable, and figure out what characters to use for path list separators
    // and path component separators. Try to go with the flow.
    //

    PathVariable = getenv("PATH");
    if (PathVariable == NULL) {
        PathVariable = getenv("Path");
        if (PathVariable == NULL) {
            goto AppSetExecNameEnd;
        }
    }

    PathVariable = strdup(PathVariable);
    if (PathVariable == NULL) {
        goto AppSetExecNameEnd;
    }

    Separator = ':';
    if (strchr(PathVariable, ';') != NULL) {
        Separator = ';';
    }

    Slash = strchr(PathVariable, '/');
    if (Slash == NULL) {
        Slash = strchr(PathVariable, '\\');
        if (Slash == NULL) {
            Slash = CK_APP_PATH_SEPARATOR;
        }
    }

    CurrentPath = PathVariable;
    while (CurrentPath != NULL) {
        NextPath = strchr(CurrentPath, Separator);
        if (NextPath != NULL) {
            *NextPath = '\0';
            NextPath += 1;
        }

        if ((*CurrentPath == '\0') ||
            ((*CurrentPath == '.') && (CurrentPath[1] == '\0'))) {

            if (WorkingDirectory == NULL) {
                WorkingDirectory = getcwd(NULL, 0);
            }

            if (WorkingDirectory != NULL) {
                CurrentPath = WorkingDirectory;
            }
        }

        PrintLength = snprintf(Path,
                               sizeof(Path),
                               "%s%c%s%s",
                               CurrentPath,
                               *Slash,
                               Argument0,
                               Suffix);

        if ((PrintLength < sizeof(Path)) && (access(Path, X_OK) == 0)) {
            Final = Path;
            goto AppSetExecNameEnd;
        }

        CurrentPath = NextPath;
    }

AppSetExecNameEnd:
    if (WorkingDirectory != NULL) {
        free(WorkingDirectory);
    }

    if (PathVariable != NULL) {
        free(PathVariable);
    }

    if (Final != NULL) {
        CkAppExecName = strdup(Final);
    }

    return;
}


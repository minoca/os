/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

#include <string.h>

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
CkpAppModuleInit (
    PCK_VM Vm
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
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the app module. It is called to make the presence of
    the module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

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

    //
    // Set the original exec name.
    //

    CkPushString(Vm, CkAppExecName, strlen(CkAppExecName));
    CkSetVariable(Vm, 0, "execName");
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


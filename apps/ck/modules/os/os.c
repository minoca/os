/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    os.c

Abstract:

    This module implements the Chalk os module, which provides functionality
    from the underlying operating system.

Author:

    Evan Green 28-Aug-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "osp.h"

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
CkpOsExit (
    PCK_VM Vm
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkOsModuleValues[] = {
    {CkTypeFunction, "exit", CkpOsExit, 1},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadOsModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the OS module. It is called to make the presence of
    the os module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CkPreloadForeignModule(Vm, "os", NULL, NULL, CkpOsModuleInit);
}

VOID
CkpOsModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the OS module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    //
    // Define the OsError exception.
    //

    CkPushString(Vm, "OsError", 7);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 0);
    CkSetVariable(Vm, 0, "OsError");

    //
    // Register the functions and definitions.
    //

    CkDeclareVariables(Vm, 0, CkOsErrnoValues);
    CkDeclareVariables(Vm, 0, CkOsIoModuleValues);
    CkDeclareVariables(Vm, 0, CkOsModuleValues);
    CkpOsInitializeInfo(Vm);
    return;
}

VOID
CkpOsExit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the exit call. It takes in an exit code, and does
    not return because the current process exits.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    This routine does not return. The process exits.

--*/

{

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    exit(CkGetInteger(Vm, 1));
    CkReturnInteger(Vm, -1LL);
    return;
}

VOID
CkpOsRaiseError (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine raises an error associated with the current errno value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    This routine does not return. The process exits.

--*/

{

    INT Error;
    PCSTR ErrorString;

    Error = errno;
    ErrorString = strerror(Error);

    //
    // Create an OsError exception.
    //

    CkPushModule(Vm, "os");
    CkGetVariable(Vm, -1, "OsError");
    CkPushString(Vm, ErrorString, strlen(ErrorString));
    CkCall(Vm, 1);

    //
    // Execute instance.errno = Error.
    //

    CkPushValue(Vm, -1);
    CkPushString(Vm, "errno", 5);
    CkPushInteger(Vm, Error);
    CkCallMethod(Vm, "__set", 2);
    CkStackPop(Vm);

    //
    // Raise the exception.
    //

    CkRaiseException(Vm, -1);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


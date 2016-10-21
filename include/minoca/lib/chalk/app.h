/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    app.h

Abstract:

    This header contains definitions for the Chalk app module.

Author:

    Evan Green 19-Oct-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the pointer to the app arguments. This must be set before the app
// module is imported.
//

extern INT CkAppArgc;
extern PSTR *CkAppArgv;

//
// Define the original application name.
//

extern PCSTR CkAppExecName;

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
CkPreloadAppModule (
    PCK_VM Vm
    );

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


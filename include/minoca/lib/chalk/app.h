/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
    PCK_VM Vm,
    PCSTR Argument0
    );

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


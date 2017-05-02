/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bundle.h

Abstract:

    This header contains definitions for the Chalk bundle module.

Author:

    Evan Green 20-Oct-2016

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
// -------------------------------------------------------- Function Prototypes
//

BOOL
CkPreloadBundleModule (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine preloads the bundle module. It is called to make the presence
    of the module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

INT
CkBundleThaw (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine reloads the modules previously saved in a bundle. The exec
    name global should be set before calling this function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns 0 if the bundle was loaded successfully.

    Returns -1 if no bundle could be found.

    Returns other values on error.

--*/

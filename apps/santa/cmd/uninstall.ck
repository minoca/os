/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uninstall.ck

Abstract:

    This module implements the uninstall command, which removes packages.

Author:

    Evan Green 18-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import cmd.install;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

var description = "Uninstalls packages from the system";

//
// ------------------------------------------------------------------ Functions
//

function
command (
    args
    )

/*++

Routine Description:

    This routine implements the uninstall command. The meat is all implemented
    in the install command.

Arguments:

    args - Supplies the arguments to the function.

Return Value:

    Returns an exit code.

--*/

{

    return (install.command)(args);
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    build.ck

Abstract:

    This module contains build helper routines used by packages.

Author:

    Evan Green 30-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import os;
from santa.config import config;
from santa.file import chdir, mkdir, rmtree;
from santa.lib.archive import Archive;
from spawn import ChildProcess, OPTION_SHELL, OPTION_CHECK;

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

//
// ------------------------------------------------------------------ Functions
//

function
shell (
    command
    )

/*++

Routine Description:

    This routine runs a command in a shell environment, and waits for the
    result. If the command fails, an exception is raised.

Arguments:

    command - Supplies either a string or a list to run.

Return Value:

    None. If the command fails, an exception is raised.

--*/

{

    var child = ChildProcess(command);

    child.options = OPTION_SHELL | OPTION_CHECK;
    child.launch();
    child.wait(-1);
    return;
}

function
autoconfigure (
    arguments,
    options
    )

/*++

Routine Description:

    This routine performs the autoconf configure step.

Arguments:

    arguments - Supplies additional arguments to pass along to the configure
        script.

    options - Supplies options that control execution.

Return Value:

    None. On failure, an exception is raised.

--*/

{

    //
    // TODO: Autoreconf, then configure...
    //
}

function
autoconf (
    arguments
    )

/*++

Routine Description:

    This routine performs a basic autoreconf and ./configure step.

Arguments:

    arguments - Supplies additional arguments to pass along to the configure
        script.

Return Value:

    None. On failure, an exception is raised.

--*/

{

    return autoconfigure(arguments, null);
}

//
// --------------------------------------------------------- Internal Functions
//


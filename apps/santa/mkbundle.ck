/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mkbundle.ck

Abstract:

    This module is a build script that can created a single executable version
    of the santa application.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from app import argv;
from bundle import create;

import santa;

import io;
import os;

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
main (
    )

/*++

Routine Description:

    This routine implements the entry point into the create bundle application.

Arguments:

    None.

Return Value:

    0 always, or an exception is raised.

--*/

{

    var ignoredModules = [null, "__main", "app", "bundle"];
    var modules;
    var command = "from santa import main;"
                  "from os import exit;"
                  "exit(main());";

    if (argv.length() != 2) {
        Core.raise(ValueError("Usage: %s output_file" % [argv[0]]));
    }

    modules = [];
    for (key in Core.modules()) {
        if (!ignoredModules.contains(key)) {
            modules.append(Core.modules()[key]);
        }
    }

    create(argv[1], modules, command);
    return 0;
}

main();

//
// --------------------------------------------------------- Internal Functions
//


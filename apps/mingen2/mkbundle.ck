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
    of the mingen application.

Author:

    30-Jan-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from app import argv;
from bundle import create;
import io;
import make;
import mingen;
import ninja;
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

    var command = "from mingen import main;"
                  "from os import exit;"
                  "exit(main());";

    if (argv.length() != 2) {
        Core.raise(ValueError("Usage: %s output_file" % [argv[0]]));
    }

    create(argv[1], [io, make, mingen, ninja, os], command);
    return 0;
}

main();

//
// --------------------------------------------------------- Internal Functions
//


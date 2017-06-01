/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    list_realms.ck

Abstract:

    This module implements the list-realms command, used to enumerate the list
    of created.

Author:

    Evan Green 1-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from getopt import gnuGetopt;

from santa.config import config;
from santa.lib.realmmanager import getRealmManager;

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

var description = "List the existing realms";

var shortOptions = "h";
var longOptions = [
    "help"
];

var usage =
    "usage: santa list-realms [options]\n"
    "This command prints a list of the realms that have been created.\n";

//
// ------------------------------------------------------------------ Functions
//

function
command (
    args
    )

/*++

Routine Description:

    This routine implements the config command.

Arguments:

    args - Supplies the arguments to the function.

Return Value:

    Returns an exit code.

--*/

{

    var argc;
    var manager;
    var name;
    var options = gnuGetopt(args[1...-1], shortOptions, longOptions);
    var realmList;
    var value;

    args = options[1];
    argc = args.length();
    options = options[0];
    for (option in options) {
        name = option[0];
        value = option[1];
        if ((name == "-h") || (name == "--help")) {
            Core.print(usage);
            return 1;

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }
    }

    if (args.length() != 0) {
        Core.raise(ValueError("Expected no arguments"));
    }

    manager = getRealmManager();
    realmList = manager.enumerateRealms();
    for (name in realmList) {
        Core.print(name);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    new_realm.ck

Abstract:

    This module implements the new-realm command, used to create realms.

Author:

    Evan Green 25-May-2017

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

var description = "Create a new working environment";

var shortOptions = "h";
var longOptions = [
    "help"
];

var usage =
    "usage: santa new-realm [options] name...\n"
    "This command creates a new realm, representing a new working \n"
    "environment. If multiple names are specified on the command line,\n"
    "multiple realms will be created. Realm names that begin with an \n"
    "underscore are reserved for Santa.\n";

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

    if (args.length() == 0) {
        Core.raise(ValueError("Expected a realm name"));
    }

    manager = getRealmManager();
    for (name in args) {
        manager.createRealm(name, {});
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


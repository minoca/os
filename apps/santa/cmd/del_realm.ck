/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    del_realm.ck

Abstract:

    This module implements the del-realm command, used to destroy realms.

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

var description = "Create a new working environment";

var shortOptions = "h";
var longOptions = [
    "help"
];

var usage =
    "usage: santa del-realm [options] name...\n"
    "This command destroys a realm and all of its data. Multiple realms can\n"
    "be specified on the command line.\n";

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
        manager.destroyRealm(name);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


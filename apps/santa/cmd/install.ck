/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    install.ck

Abstract:

    This module implements the install command, which adds new packages to the
    system.

Author:

    Evan Green 18-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from getopt import gnuGetopt;
from santa.config import config;
from santa.lib.pkgman import PackageManager;

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

var description = "Install new packages on the system";

var shortOptions = "c:hr:v";
var longOptions = [
    "cross=",
    "help",
    "root=",
    "verbose"
];

var usage =
    "usage: santa VERB packages...\n"
    "This command VERBs packages. Options are:\n"
    "  -c, --cross=[arch-]os -- VERB an alternate arch/os (eg \"Minoca\" or "
    "\"x86_64-Minoca\"\n"
    "  -r, --root=dir -- VERB packages at the specified directory\n"
    "  -v, --verbose -- Print out more information about what's going on.\n"
    "  -h, --help -- Print this help text.\n";

//
// ------------------------------------------------------------------ Functions
//

function
command (
    args
    )

/*++

Routine Description:

    This routine implements the install/uninstall command.

Arguments:

    args - Supplies the arguments to the function.

Return Value:

    Returns an exit code.

--*/

{

    var argc;
    var build;
    var name;
    var options = gnuGetopt(args[1...-1], shortOptions, longOptions);
    var packageManager;
    var parameters = {};
    var status;
    var value;
    var verb = args[0];

    args = options[1];
    argc = args.length();
    options = options[0];
    for (option in options) {
        name = option[0];
        value = option[1];
        if ((name == "-c") || (name == "--cross")) {
            parameters.os = value.split("-", 1);
            if (parameters.os.length() == 1) {
                parameters.os = parameters.os[0];

            } else {
                parameters.arch = parameters.os[0];
                parameters.os = parameters.os[1];
            }

        } else if ((name == "-h") || (name == "--help")) {
            Core.print(usage.replace("VERB", verb, -1));
            return 1;

        } else if ((name == "-r") || (name == "--root")) {
            parameters.root = value;

        } else if ((name == "-v") || (name == "--verbose")) {
            config.setKey("core.verbose", true);

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }
    }

    if (args.length() < 1) {
        Core.raise(ValueError("Expected a package name"));
    }

    //
    // Add or remove each package.
    //

    packageManager = PackageManager(null);
    for (arg in args) {
        if (verb == "uninstall") {
            packageManager.uninstall(arg, parameters);

        } else {
            packageManager.install(arg, parameters);
        }
    }

    //
    // The plan is made, do it.
    //

    packageManager.commit();
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mingen.ck

Abstract:

    This module implements the Minoca build generator application, which can
    transform a build specification into a Makefile or Ninja file.

Author:

    Evan Green 30-Jan-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from app import argv;
from getopt import gnuGetopt;
import os;
from os import getcwd;

from santa.config import loadConfig;
from santa.file import createStandardPaths;
from santa.modules import enumerateCommands, initModuleSupport, runCommand;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

var VERSION_MAJOR = 1;
var VERSION_MINOR = 0;

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

function
printHelp (
    );

//
// -------------------------------------------------------------------- Globals
//

var shortOptions = "+f:r:hvV";
var longOptions = [
    "file=",
    "root=",
    "help",
    "verbose",
    "version"
];

var usage =
    "usage: santa [global_options] command [command_options]\n"
    "Santa is a package manager. It can build, install, and maintain \n"
    "packages and environments.\n"
    "Global options:\n"
    "  -f, --file=config -- Use the config file at the given path.\n"
    "  -r, --root=dir -- Set the given directory as the root directory\n"
    "      before doing anything else. Used for offline installs.\n"
    "  -h, --help -- Shows this help.\n"
    "  -v, --verbose -- Print more information.\n"
    "  -V, --version -- Print the version of this application and exit.\n";

//
// ------------------------------------------------------------------ Functions
//

function
main (
    )

/*++

Routine Description:

    This routine implements the application entry point for the mingen
    application.

Arguments:

    None.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    var appOptions = gnuGetopt(argv[1...-1], shortOptions, longOptions);
    var args = appOptions[1];
    var command;
    var configpath = null;
    var entries = {};
    var help = false;
    var name;
    var override;
    var status;
    var value;

    override = {
        "core": {}
    };

    //
    // The return from getopt is [config, remainingArgs]. Get the config now.
    //

    appOptions = appOptions[0];
    for (option in appOptions) {
        name = option[0];
        value = option[1];
        if ((name == "-f") || (name == "--file")) {
            configpath = value;

        } else if ((name == "-r") || (name == "--root")) {
            override.core.root = value;

        } else if ((name == "-h") || (name == "--help")) {
            help = true;

        } else if ((name == "-v") || (name == "--verbose")) {
            override.core.verbose = true;

        } else if ((name == "-V") || (name == "--version")) {
            Core.print("Santa version %d.%d" % [VERSION_MAJOR, VERSION_MINOR]);
            return 1;

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }
    }

    //
    // Load up the global config.
    //

    loadConfig(configpath, override);
    initModuleSupport();
    if (args.length() == 0) {
        help = true;
        Core.print("Error: expected a command.");
    }

    if (help) {
        printHelp();
        return 1;
    }

    createStandardPaths();
    command = args[0];
    status = runCommand(command, args);
    return status;
}

//
// --------------------------------------------------------- Internal Functions
//

function
printHelp (
    )

/*++

Routine Description:

    This routine prints the application usage.

Arguments:

    None.

Return Value:

    None.

--*/

{

    var commands;

    Core.print(usage);
    commands = enumerateCommands();
    Core.print("Valid commands are:");
    for (command in commands) {
        Core.print("%-20s %s" % [command.name, command.description]);
    }

    return;
}

if ((os.basename)(argv[0]).contains("santa")) {
    main();
}


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    build.ck

Abstract:

    This module implements the build command, which will build a package based
    on an input recipe.

Author:

    Evan Green 3-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from getopt import gnuGetopt;
from santa.config import config;
from santa.lib.build import Build;

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

var description = "Build a package from its sources";

var shortOptions = "hs:r:v";
var longOptions = [
    "help",
    "solo-step=",
    "run=",
    "verbose"
];

var usage =
    "usage: santa build [options] <recipe.ck>\n"
    "This command builds a package from source. The input file is a recipe\n"
    "specifying definitions for the various build steps. Options are:\n"
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

    This routine implements the convert-archive command.

Arguments:

    args - Supplies the arguments to the function.

Return Value:

    Returns an exit code.

--*/

{

    var argc;
    var build;
    var inputPath;
    var ignoreErrors = false;
    var mode = "r";
    var name;
    var options = gnuGetopt(args[1...-1], shortOptions, longOptions);
    var runTo;
    var soloStep;
    var status;
    var value;
    var verbose = config.getKey("core.verbose");

    args = options[1];
    argc = args.length();
    options = options[0];
    for (option in options) {
        name = option[0];
        value = option[1];
        if ((name == "-h") || (name == "--help")) {
            Core.print(usage);
            return 1;

        } else if ((name == "-s") || (name == "--solo-step")) {
            soloStep = value;

        } else if ((name == "-r") || (name == "--run")) {
            runTo = value;

        } else if ((name == "-v") || (name == "--verbose")) {
            verbose = true;

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }
    }

    if (args.length() < 1) {
        Core.raise(ValueError("Expected an input recipe path."));
    }

    //
    // Build each recipe.
    //

    for (arg in args) {
        if (verbose) {
            Core.print("Building recipe: %s" % arg);
        }

        build = Build(arg);
        if (verbose) {
            build.vars.verbose = verbose;
        }

        //
        // Run a single step directly if requested.
        //

        if (soloStep) {
            build.runStep(soloStep);

        //
        // Run up to a certain point if requested.
        //

        } else if (runTo) {
            while (build.step != runTo) {
                build.runStep(build.step);
            }

        //
        // Run to completion.
        //

        } else {
            build.run();
        }
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


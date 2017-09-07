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
import os;
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

var shortOptions = "c:his:o:r:v";
var longOptions = [
    "cross=",
    "help",
    "ignore-missing-deps",
    "output="
    "solo-step=",
    "run=",
    "verbose"
];

var usage =
    "usage: santa build [options] <recipe.ck|build_number...>\n"
    "This command builds a package from source. The input parameter is either\n"
    "a path to a recipe file or a previously incomplete build number.\n"
    "Options are:\n"
    "  -c, --cross=[arch-]os -- Cross compile (eg \"Minoca\" or "
    "\"x86_64-Minoca\"\n"
    "  -i, --ignore-missing-deps -- Ignore missing dependencies\n"
    "  -o, --output=dir -- Output packages to the specified directory\n"
    "  -r, --run=step -- Run to the specified step and stop.\n"
    "  -s, --solo-step=step -- Run only the specified step and stop.\n"
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

    This routine implements the build command.

Arguments:

    args - Supplies the arguments to the function.

Return Value:

    Returns an exit code.

--*/

{

    var argc;
    var build;
    var crossArch;
    var crossOs;
    var inputPath;
    var ignoreMissingDeps = false;
    var mode = "r";
    var name;
    var options = gnuGetopt(args[1...-1], shortOptions, longOptions);
    var output;
    var runTo;
    var soloStep;
    var startDirectory = (os.getcwd)();
    var status;
    var value;
    var verbose = config.getKey("core.verbose");

    args = options[1];
    argc = args.length();
    options = options[0];
    for (option in options) {
        name = option[0];
        value = option[1];
        if ((name == "-c") || (name == "--cross")) {
            crossOs = value.split("-", 1);
            if (crossOs.length() == 1) {
                crossOs = crossOs[0];

            } else {
                crossArch = crossOs[0];
                crossOs = crossOs[1];
            }

        } else if ((name == "-h") || (name == "--help")) {
            Core.print(usage);
            return 1;

        } else if ((name == "-i") || (name == "--ignore-missing-deps")) {
            ignoreMissingDeps = true;

        } else if ((name == "-o") || (name == "--output")) {
            output = value;

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
        try {
            arg = Int.fromString(arg);
            if (verbose) {
                Core.print("Resuming build: %d" % arg);
            }

        } except ValueError {
            if (verbose) {
                Core.print("Building recipe: %s" % arg);
            }
        }

        build = Build(arg);
        if (crossOs) {
            build.vars.os = crossOs;
        }

        if (crossArch) {
            build.vars.arch = crossArch;
        }

        if (output) {
            build.outdir = output;
        }

        build.ignoreMissingDeps = ignoreMissingDeps;
        Core.print("%s build %d" % [(arg is String) ? "starting" : "resuming",
                                    build.number]);

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

        //
        // Return back to the original working directory in case the next
        // argument is a relative path.
        //

        (os.chdir)(startDirectory);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


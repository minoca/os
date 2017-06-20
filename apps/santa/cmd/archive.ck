/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archive.ck

Abstract:

    This module implements the archive command, a low level command used to
    create and extract archives.

Author:

    Evan Green 20-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from getopt import gnuGetopt;

from os import chdir;
from santa.config import config;
from santa.lib.archive import Archive;

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

var description = "Low-level command to create and extract archives";

var shortOptions = "C:cf:txh";
var longOptions = [
    "create",
    "directory=",
    "file=",
    "list",
    "extract",
    "help"
];

var usage =
    "usage: santa archive [-t|-x|-c] -f <archive> [options] [files...]\n"
    "This low-level command directly creates, lists, or extracts the contents\n"
    "of an archive. Specify one of -c, -x, or -t, for create, extract, or \n"
    "list respectively. Options are:\n"
    "  -C, --directory=dir -- Change to the given directory before \n"
    "      processing archive members.\n"
    "  -f, --file=archive -- Specifies the archive name. Required.\n"
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

    This routine implements the config command.

Arguments:

    args - Supplies the arguments to the function.

Return Value:

    Returns an exit code.

--*/

{

    var archive;
    var argc;
    var command = [];
    var directory;
    var mode = "r";
    var name;
    var options = gnuGetopt(args[1...-1], shortOptions, longOptions);
    var path;
    var value;

    args = options[1];
    argc = args.length();
    options = options[0];
    for (option in options) {
        name = option[0];
        value = option[1];
        if ((name == "-c") || (name == "--create")) {
            command.append("create");

        } else if ((name == "-C") || (name == "--directory")) {
            directory = value;

        } else if ((name == "-f") || (name == "--file")) {
            path = value;

        } else if ((name == "-t") || (name == "--list")) {
            command.append("list");

        } else if ((name == "-x") || (name == "--extract")) {
            command.append("extract");

        } else if ((name == "-h") || (name == "--help")) {
            Core.print(usage);
            return 1;

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }
    }

    if (path == null) {
        Core.raise(ValueError("Expected an archive name via -f <archive>"));
    }

    if (command.length() != 1) {
        Core.raise(ValueError("Expected exactly one of -c, -t, or -x."));
    }

    command = command[0];
    if (command == "create") {
        mode = "w";
    }

    archive = Archive.open(path, mode);
    if (directory) {
        chdir(directory);
    }

    if (command == "create") {
        for (arg in args) {
            archive.add(arg, arg);
        }

    } else if (command == "extract") {
        if (args.length()) {
            for (arg in args) {
                archive.extract(arg, arg);
            }

        } else {
            archive.cpio.extractAll(null, null, -1, -1, true);
        }

    } else if (command == "list") {
        archive.cpio.list(null, true);
    }

    archive.close();
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


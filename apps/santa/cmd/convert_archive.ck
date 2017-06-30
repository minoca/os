/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    convert_archive.ck

Abstract:

    This module implements the convert-archive command, a sideband command used
    to convert other archives into .cpio.lz archives.

Author:

    Evan Green 29-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from getopt import gnuGetopt;

import os;
from santa.config import config;
from santa.file import mkdir, rmtree;
from santa.lib.archive import Archive;
from spawn import ChildProcess, OPTION_SHELL;

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

function
determineArchiveFormat (
    input
    );

function
getArchiveFormat (
    format
    );

//
// -------------------------------------------------------------------- Globals
//

var description = "Sideband command to convert archives into .cpio.lz format";

var shortOptions = "C:ef:ho:v";
var longOptions = [
    "directory=",
    "format=",
    "help",
    "ignore-errors",
    "output=",
    "verbose"
];

var usage =
    "usage: santa convert-archive [options] input_archive [output_archive]\n"
    "This sideband command is useful for converting archives in other \n"
    "formats into .cpio.lz archives, the native archive type used by Santa.\n"
    "Options are:\n"
    "  -C, --directory=dir -- Use the given directory for temporary "
    "extraction.\n"
    "  -e, --ignore-errors -- Ignore errors from the run commands.\n"
    "  -f, --format=format -- Supply the file extensions of the input \n"
    "      archive, in case they cannot be guessed based on the input file \n"
    "      name. Examples include .tar.gz or .cpio.bz2"
    "  -o, --output=file -- Specifies the output file name.\n"
    "  -v, --verbose -- Print out more information about what's going on.\n"
    "  -h, --help -- Print this help text.\n";

var convertArchiveFormats = {
    "tar": ["tar", "none"],
    "tar.gz": ["tar", "gz"],
    "tgz": ["tar", "gz"],
    "tar.xz": ["tar", "xz"],
    "tar.bz2": ["tar", "bz2"],
};

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

    var commandLine;
    var compressOption;
    var archive;
    var argc;
    var directory;
    var inputPath;
    var ignoreErrors = false;
    var format;
    var mode = "r";
    var name;
    var options = gnuGetopt(args[1...-1], shortOptions, longOptions);
    var outputPath;
    var process;
    var status;
    var tempdir;
    var value;
    var verbose = false;

    args = options[1];
    argc = args.length();
    options = options[0];
    for (option in options) {
        name = option[0];
        value = option[1];
        if ((name == "-C") || (name == "--directory")) {
            directory = value;

        } else if ((name == "-f") || (name == "--format")) {
            format = value;

        } else if ((name == "-e") || (name == "--ignore-errors")) {
            ignoreErrors = true;

        } else if ((name == "-h") || (name == "--help")) {
            Core.print(usage);
            return 1;

        } else if ((name == "-o") || (name == "--output")) {
            outputPath = value;

        } else if ((name == "-v") || (name == "--verbose")) {
            verbose = true;

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }
    }

    if (args.length() < 1) {
        Core.raise(ValueError("Expected an input archive."));

    } else {
        inputPath = args[0];
        if (args.length() >= 2) {
            outputPath = args[1];
            if (args.length() > 2) {
                Core.raise(ValueError("Expected at most 2 arguments"));
            }
        }
    }

    //
    // Determine the input format.
    //

    if (format) {
        format = determineArchiveFormat(format);

    } else {
        format = determineArchiveFormat(inputPath);
    }

    //
    // Create the temporary working directory.
    //

    if (directory == null) {
        directory = "convert%d" % (os.getpid)();
        if (verbose) {
            Core.print("Creating %s" % directory);
        }

        mkdir(directory);
        tempdir = directory;
    }

    if (format[0] == "tar") {
        compressOption = "";
        if (format[1] == "gz") {
            compressOption = "-z ";

        } else if (format[1] == "xz") {
            compressOption = "-J ";

        } else if (format[1] == "bzip2") {
            compressOption = "-j ";

        } else if (format[1] != "none") {
            Core.raise(ValueError("Unknown compression format '%s'" %
                                  format[1]));
        }

        commandLine = "tar -x %s-C %s -f %s" %
                      [compressOption, directory, inputPath];

    } else {
        Core.raise(ValueError("Unknown archive format '%s'" % format[0]));
    }

    if (verbose) {
        Core.print("Extracting: %s" % commandLine);
    }

    //
    // Run the process to extract.
    //

    process = ChildProcess(commandLine);
    process.options |= OPTION_SHELL;
    process.launch();
    status = process.wait(-1);
    if (verbose) {
        Core.print("Extraction finished with status %d" % status);
    }

    if ((!ignoreErrors) && (status != 0)) {
        Core.print("Extraction command: %s" % commandLine);
        Core.print("Extraction directory: %s" % directory);
        Core.print("Error: Extraction failed with return code %d" % status);
        return status;
    }

    if (outputPath == null) {
        outputPath = format[2];
    }

    //
    // Create an archive and add the members.
    //

    if (verbose) {
        Core.print("Creating archive %s" % outputPath);
    }

    archive = Archive.open(outputPath, "w");
    archive.add(directory, "");
    archive.close();
    if (verbose) {
        Core.print("Finished creating %s" % outputPath);
    }

    if (tempdir) {
        if (verbose) {
            Core.print("Removing temporary directory: %s" % tempdir);
        }

        rmtree(tempdir);
    }

    if (verbose) {
        Core.print("Finished");
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

function
determineArchiveFormat (
    input
    )

/*++

Routine Description:

    This routine converts an input file path into a list of well known formats.

Arguments:

    input - Supplies the input file path, or the input format.

Return Value:

    Returns a list of [archive, compression, output] format strings.

    Raises an exception if not recognized.

--*/

{

    var left = null;
    var originalInput = input;
    var result;
    var split;

    //
    // Try the whole thing.
    //

    result = getArchiveFormat(input);
    if (result) {
        return [result[0], result[1], null];
    }

    //
    // Loop chopping up to the first dot.
    //

    split = input.split(".", 1);
    left = null;
    while (split.length() > 1) {
        input = split[1];
        if (left == null) {
            left = split[0];

        } else {
            left = "%s.%s" % [left, split[0]];
        }

        result = getArchiveFormat(split[1]);
        if (result) {
            return [result[0], result[1], "%s.cpio.lz" % left];
        }

        split = input.split(".", 1);
    }

    Core.raise(ValueError("Archive format cannot be determined from '%s'" %
                          originalInput));
}

function
getArchiveFormat (
    format
    )

/*++

Routine Description:

    This routine converts a potential ending into a list of well known formats.

Arguments:

    format - Supplies the format string, without a leading ., but with any
        separating dots.

Return Value:

    Returns a list of [archive, compression] format strings.

    null if not recognized.

--*/

{

    var entry;

    try {
        entry = convertArchiveFormats[format];

    } except KeyError {
        return null;
    }

    return entry;
}


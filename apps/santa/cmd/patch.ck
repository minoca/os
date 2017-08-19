/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    patch.ck

Abstract:

    This module implements the patch command, which contains a suite of
    subcommands to help the user manage a set of patches for a package build.

Author:

    Evan Green 10-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from getopt import gnuGetopt;

from santa.config import config;
from santa.lib.patchman import PatchManager;

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

var description = "Patch management command suite";

var markShortOptions = "hu";
var markLongOptions = [
    "help",
    "unapplied"
];

var deleteShortOptions = "hs";
var deleteLongOptions = [
    "help",
    "shift"
];

var commitShortOptions = "fhm:";
var commitLongOptions = [
    "force",
    "help",
    "message="
];

var usage =
    "usage: santa patch start <src_dir> <patch_dir>\n"
    "       santa patch add <files...>\n"
    "       santa patch remove <files...>\n"
    "       santa patch apply <numbers...>\n"
    "       santa patch unapply <numbers...>\n"
    "       santa patch mark [--unapplied] <numbers...>\n"
    "       santa patch goto <number>\n"
    "       santa patch commit [options] <name>\n"
    "       santa patch delete [--shift] [numbers...]\n"
    "       santa patch info\n"
    "       santa patch edit <number>\n"
    "The santa patch command is a suite of commands designed to help manage a\n"
    "set of patches for a package.\n"
    "Use santa patch start to begin working on a new source directory,\n"
    "also specifying where patches should go (or already are).\n"
    "Use santa patch add to add files to the current patch being developed.\n"
    "Files must be explicitly added before modification for inclusion during\n"
    "commit.\n"
    "Use santa remove to remove files previously added from the uncommitted\n"
    "patch.\n"
    "Use santa patch apply to apply one or more patches in the patch "
    "directory.\n"
    "Use santa patch unapply to remove the effects of one or more patches in\n"
    "the patch directory.\n"
    "Use santa patch mark to mark a set of patches as applied or unapplied \n"
    "without actually making any changes. Useful for manual adjustments.\n"
    "Use santa patch goto to apply all patches up to and including the \n"
    "specified patch number.\n"
    "Use santa patch commit to create a new patch file in the patch \n"
    "directory containing the differences between all added files and their \n"
    "current contents in the source directory. The new patch is marked as \n"
    "applied, and the set of files added is cleared.\n"
    "Specify -m to add a commit message.\n"
    "Specify -f to replace a patch that already exists for that number.\n"
    "Use santa patch delete to remove one or more patches from the patch\n"
    "directory. If --shift is specified, the remaining patches will be\n"
    "renamed to fill in the gap.\n"
    "Use santa patch info to get information on the current patch\n"
    "Use santa patch edit to revert to the change before the specified \n"
    "number, add the files from the numbered patch, and apply the patch. \n"
    "This is useful if you want to modify an existing patch, as the working \n"
    "state can be changed and the committed.\n";

//
// ------------------------------------------------------------------ Functions
//

function
command (
    args
    )

/*++

Routine Description:

    This routine implements the archive command.

Arguments:

    args - Supplies the arguments to the function.

Return Value:

    Returns an exit code.

--*/

{

    var applied = true;
    var command;
    var force = false;
    var manager = PatchManager.load();
    var message;
    var name;
    var options;
    var shift = false;
    var value;

    if (args.length() < 2) {
        Core.raise(ValueError("Expected a command. See --help for usage"));
    }

    command = args[1];

    //
    // Don't ignore a cry for help.
    //

    if ((command == "-h") || (command == "--help")) {
        Core.print(usage);
        return 1;

    //
    // Add new files to the current patch.
    //

    } else if (command == "add") {
        if (args.length() <= 2) {
            manager.add(manager.srcdir);

        } else {
            for (arg in args[2...-1]) {
                manager.add(arg);
            }
        }

    //
    // Remove files from the current pending patch.
    //

    } else if (command == "remove") {
        if (args.length() <= 2) {
            manager.remove(manager.srcdir);

        } else {
            for (arg in args[2...-1]) {
                manager.remove(arg);
            }
        }

    //
    // Reset the patch manager state.
    //

    } else if (command == "start") {
        if (args.length() != 4) {
            Core.raise(ValueError("Expected 2 arguments. "
                                  "Try --help for usage"));
        }

        manager = PatchManager(args[2], args[3]);

    //
    // Apply or remove one or more patches.
    //

    } else if ((command == "apply") || (command == "unapply")) {
        for (arg in args[2...-1]) {
            arg = Int.fromString(arg);
            if (command == "unapply") {
                arg = -arg;
            }

            manager.applyPatch(arg);
            manager.save();
        }

    //
    // Apply patches up to or down to a given number.
    //

    } else if (command == "goto") {
        if (args.length() != 3) {
            Core.raise(ValueError("Expected exactly one argument. "
                                  "Try --help for usage"));
        }

        manager.applyTo(Int.fromString(args[2]));

    //
    // Mark patches as applied without doing anything.
    //

    } else if (command == "mark") {
        options = gnuGetopt(args[2...-1],
                            markShortOptions,
                            markLongOptions);

        for (option in options[0]) {
            name = option[0];
            value = option[1];
            if ((name == "-h") || (name == "--help")) {
                Core.print(usage);
                return 1;

            } else if ((name == "-u") || (name == "--unapplied")) {
                applied = false;

            } else {
                Core.raise(ValueError("Invalid option '%s'" % name));
            }
        }

        if (options[1].length() == 0) {
            manager.markPatches(-1, applied);

        } else {
            for (arg in options[1]) {
                manager.markPatches(Int.fromString(arg), applied);
            }
        }

    //
    // Create a patch from the currently added files.
    //

    } else if (command == "commit") {
        options = gnuGetopt(args[2...-1],
                            commitShortOptions,
                            commitLongOptions);

        for (option in options[0]) {
            name = option[0];
            value = option[1];
            if ((name == "-f") || (name == "--force")) {
                force = true;

            } else if ((name == "-h") || (name == "--help")) {
                Core.print(usage);
                return 1;

            } else if ((name == "-m") || (name == "--message")) {
                message = value;

            } else {
                Core.raise(ValueError("Invalid option '%s'" % name));
            }
        }

        args = options[1];
        if (args.length() != 1) {
            Core.raise(ValueError("Expected an argument. "
                                  "Try --help for usage"));
        }

        manager.commit(args[0], message, force);

    //
    // Remove a patch from the directory.
    //

    } else if (command == "delete") {
        options = gnuGetopt(args[2...-1],
                            deleteShortOptions,
                            deleteLongOptions);

        for (option in options[0]) {
            name = option[0];
            value = option[1];
            if ((name == "-h") || (name == "--help")) {
                Core.print(usage);
                return 1;

            } else if ((name == "-s") || (name == "--shift")) {
                shift = true;

            } else {
                Core.raise(ValueError("Invalid option '%s'" % name));
            }
        }

        args = options[1];
        for (arg in args) {
            manager.deletePatch(Int.fromString(arg), shift);
            manager.save();
        }

    //
    // Print information about the current patch.
    //

    } else if (command == "info") {
        Core.print("Source Directory: %s\nPatch Directory: %s\nFiles:" %
                   [manager.srcdir, manager.patchdir]);

        for (file in manager.files) {
            Core.print("\t%s" % file);
        }

        Core.print("Current Diff:");
        Core.print(manager.currentDiffSet().unifiedDiff());

    //
    // Revert to before the specified path, then re-add the changes of the
    // patch.
    //

    } else if (command == "edit") {
        if (args.length() != 3) {
            Core.raise(ValueError("Expected exactly one argument. "
                                  "Try --help for usage"));
        }

        manager.edit(Int.fromString(args[2]));

    } else {
        Core.raise(ValueError("Unknown command %s" % command));
    }

    //
    // Save the patch manager state for the next run.
    //

    manager.save();
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//


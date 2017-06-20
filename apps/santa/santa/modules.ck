/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    modules.ck

Abstract:

    This module is responsible for loading and maintaining a list of other
    loadable modules within Santa.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from os import listdir, OsError;

from santa.config import config;
from santa.file import path;

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
_enumerateModules (
    type,
    functionName
    );

//
// -------------------------------------------------------------------- Globals
//

var builtinCommands = [
    ["archive", "Low-level archive creation/extraction"],
    ["config", "Get or set configuration parameters"],
    ["new-realm", "Create a new working environment"],
    ["del-realm", "Destroy and delete a realm"],
];

//
// Other possible containment types not yet implemented:
// cgroups.
//

var builtinContainment = [
    ["none", "No containment"],
    ["chroot", "Directory containment"],
];

//
// Other storage types not yet implemented:
// romount (returns paths to a read-only mount of the store except during
// installation).
//

var builtinStorage = [
    ["none", "No storage"],
    ["basic", "Basic package contents store"],
];

//
// Other presentation types not yet implemented:
// hardlink - Create hard links from storage to the final location.
// symlink - Create symlinks from storage to the final location.
// overlay - Use overlay file systems of storage directories to create the
// final environment.
//

var builtinPresentation = [
    ["copy", "Copy-based package installation"],
];

//
// ------------------------------------------------------------------ Functions
//

function
initModuleSupport (
    )

/*++

Routine Description:

    This routine initializes support for loadable modules.

Arguments:

    None.

Return Value:

    None.

--*/

{

    var extensions = config.getKey("core.extensions");
    var fullpath;
    var modulePath = Core.modulePath();
    var verbose = config.getKey("core.verbose");;

    if (extensions == null) {
        return;
    }

    if (!(extensions is List)) {
        Core.raise(ValueError("config.extensions must be a list"));
    }

    for (extension in extensions) {
        extension = extension.__str();
        fullpath = path(extension);
        if (verbose) {
            Core.print("Adding %s to module path" % fullpath);
        }

        modulePath.append(fullpath);
    }

    return;
}

function
runCommand (
    command,
    args
    )

/*++

Routine Description:

    This routine runs the given Santa command.

Arguments:

    command - Supplies the name of the command to run.

    args - Supplies the arguments to the command.

Return Value:

    None.

--*/

{

    var commandModule;
    var module;
    var run;

    commandModule = command.replace("-", "_", -1);
    try {
        module = Core.importModule("cmd." + commandModule);
        module.run();

    } except ImportError {
        Core.print("Error: Unknown command %s. Try santa --help for usage" %
                   command);

        return 127;
    }

    run = module.command;
    return run(args);
}

function
enumerateCommands (
    )

/*++

Routine Description:

    This routine returns a list of available commands.

Arguments:

    None.

Return Value:

    Returns a list of commands.

--*/

{

    var commandModule;
    var module;
    var result;

    result = [];
    for (builtin in builtinCommands) {
        commandModule = builtin[0].replace("-", "_", -1);
        module = Core.importModule("cmd." + commandModule);
        module.run();
        result.append({
            "value": module.command,
            "name": builtin[0],
            "description": builtin[1]
        });
    }

    result += _enumerateModules("cmd", "command");
    return result;
}

function
getContainment (
    name
    )

/*++

Routine Description:

    This routine returns the containment class associated with the given name.

Arguments:

    name - Supplies the name of the containment class.

Return Value:

    None.

--*/

{

    var module;
    var run;

    try {
        module = Core.importModule("containment." + name);
        module.run();

    } except ImportError {
        Core.print("Error: Unknown containment type %s.");
        return 2;
    }

    return module.containment;
}

function
enumerateContainmentTypes (
    )

/*++

Routine Description:

    This routine returns a list of available containment types.

Arguments:

    None.

Return Value:

    Returns a list of containment types.

--*/

{

    var module;
    var result;

    result = [];
    for (builtin in builtinContainment) {
        module = Core.importModule("containment." + builtin[0]);
        module.run();
        result.append({
            "value": module.containment,
            "name": builtin[0],
            "description": builtin[1]
        });
    }

    result += _enumerateModules("containment", "containment");
    return result;
}

function
getStorage (
    name
    )

/*++

Routine Description:

    This routine returns the storage class associated with the given name.

Arguments:

    name - Supplies the name of the containment class.

Return Value:

    None.

--*/

{

    var module;
    var run;

    try {
        module = Core.importModule("storage." + name);
        module.run();

    } except ImportError {
        Core.print("Error: Unknown storage type %s.");
        return 2;
    }

    return module.storage;
}

function
enumerateStorageTypes (
    )

/*++

Routine Description:

    This routine returns a list of available storage methods.

Arguments:

    None.

Return Value:

    Returns a list of storage methods.

--*/

{

    var module;
    var result;

    result = [];
    for (builtin in builtinStorage) {
        module = Core.importModule("storage." + builtin[0]);
        module.run();
        result.append({
            "value": module.storage,
            "name": builtin[0],
            "description": builtin[1]
        });
    }

    result += _enumerateModules("storage", "storage");
    return result;
}

function
getPresentation (
    name
    )

/*++

Routine Description:

    This routine returns the presentation class associated with the given name.

Arguments:

    name - Supplies the name of the containment class.

Return Value:

    None.

--*/

{

    var module;
    var run;

    try {
        module = Core.importModule("presentation." + name);
        module.run();

    } except ImportError {
        Core.print("Error: Unknown presentation type %s.");
        return 2;
    }

    return module.presentation;
}

function
enumeratePresentationTypes (
    )

/*++

Routine Description:

    This routine returns a list of available presentation methods.

Arguments:

    None.

Return Value:

    Returns a list of storage methods.

--*/

{

    var module;
    var result;

    result = [];
    for (builtin in builtinPresentation) {
        module = Core.importModule("presentation." + builtin[0]);
        module.run();
        result.append({
            "value": module.presentation,
            "name": builtin[0],
            "description": builtin[1]
        });
    }

    result += _enumerateModules("presentation", "presentation");
    return result;
}

//
// --------------------------------------------------------- Internal Functions
//

function
_enumerateModules (
    type,
    functionName
    )

/*++

Routine Description:

    This routine enumerates all modules in the extensions directory of the
    given type.

Arguments:

    type - Supplies the directory within the extension folder to search.

    functionName - Supplies the function required to implement the module.

Return Value:

    None.

--*/

{

    var callable;
    var description;
    var directory;
    var extensions = config.getKey("core.extensions");
    var fullpath;
    var module;
    var name;
    var result;
    var results;
    var verbose = config.getKey("core.verbose");

    results = [];
    if (extensions == null) {
        return results;
    }

    if (!(extensions is List)) {
        Core.raise(ValueError("config.extensions must be a list"));
    }

    for (extension in extensions) {
        extension = extension.__str();
        fullpath = path(extension + "/" + type);
        try {
            directory = listdir(fullpath);

        } except OsError as e {
            if (verbose) {
                Core.print("No modules at %s: %s" % [fullpath, e.args]);
            }

            continue;
        }

        for (entry in directory) {
            if (entry[-3...-1] != ".ck") {
                if (verbose) {
                    Core.print("Skipping non-module %s/%s" % [fullpath, entry]);
                    continue;
                }
            }

            name = entry[0..-3];
            try {
                module = Core.importModule("%s.%s" % [type, name]);

            } except Exception as e {
                if (verbose) {
                    Core.print("Module %s/%s failed to load: %s" %
                               [fullpath, entry, e]);
                }

                continue;
            }

            try {
                callable = module.__get(functionName);
                description = module.description;
                result = {
                    "value": callable,
                    "name": name,
                    "description": description
                };

                results.append(result);

            } except KeyError {
                if (verbose) {
                    Core.print("Module %s/%s does not implement %s "
                               "(or description)" %
                               [fullpath, entry, functionName]);
                }
            }
        }
    }

    return results;
}

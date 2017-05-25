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
    ["config", "Get or set configuration parameters"],
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

    None.

--*/

{

    var module;
    var result;

    result = [];
    for (builtin in builtinCommands) {
        module = Core.importModule("cmd." + builtin[0]);
        result.append({
            "function": module.command,
            "name": builtin[0],
            "description": builtin[1]
        });
    }

    result += _enumerateModules("cmd", "command");
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
                    "function": callable,
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

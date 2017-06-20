/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    config.ck

Abstract:

    This module implements the config command for Santa.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import io;
from getopt import gnuGetopt;
from json import dumps, loads;

from santa.config import config;
from santa.lib.santaconfig import SantaConfig;

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
_listConfig (
    actor,
    json
    );

function
_printConfigValue (
    prefix,
    value
    );

//
// -------------------------------------------------------------------- Globals
//

var description = "Get or set application parameters";

var shortOptions = "af:hjlrsu";
var longOptions = [
    "add",
    "file=",
    "help",
    "json",
    "list",
    "remove",
    "system",
    "user"
];

var usage =
    "usage: santa config [options] key\n"
    "       santa config [options] key value [value...]\n"
    "santa config gets or sets an application configuration value.\n"
    "Valid options are:\n"
    "  -f, --file=file -- Work on the specified config file.\n"
    "  -j, --json -- Print the result in JSON or read the value as JSON.\n"
    "  -s, --system -- Work on the system store rather than the user's store.\n"
    "  -u, --user -- Work on the user store instead of the combination of "
    "all stores\n"
    "  -r, --remove -- Remove the given key\n"
    "  -a, --add -- Add to the given key instead of replacing it.\n"
    "  -l, --list -- List all values found in the store.\n";

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

    var action = null;
    var actioncount = 0;
    var actor = config;
    var argc;
    var json = false;
    var list = false;
    var name;
    var options = gnuGetopt(args[1...-1], shortOptions, longOptions);
    var previous;
    var value;

    args = options[1];
    argc = args.length();
    options = options[0];
    for (option in options) {
        name = option[0];
        value = option[1];
        if ((name == "-a") || (name == "--add")) {
            action = "add";
            actioncount += 1;

        } else if ((name == "-f") || (name == "--file")) {
            config.__init(value, {});

        } else if ((name == "-s") || (name == "--system")) {
            actor = actor._global;

        } else if ((name == "-h") || (name == "--help")) {
            Core.print(usage);
            return 1;

        } else if ((name == "-j") || (name == "--json")) {
            json = true;

        } else if ((name == "-l") || (name == "--list")) {
            list = true;

        } else if ((name == "-r") || (name == "--remove")) {
            action = "remove";
            actioncount += 1;

        } else if ((name == "-u") || (name == "--user")) {
            actor = actor._user;

        } else {
            Core.raise(ValueError("Invalid option '%s'" % name));
        }
    }

    if (list) {
        if ((argc != 0) || (actioncount != 0)) {
            Core.raise(ValueError("Expected no arguments with --list"));
        }

        _listConfig(actor, json);
        return 0;
    }

    if (actioncount > 1) {
        Core.raise(ValueError("Specify only one action"));
    }

    if (argc == 0) {
        Core.raise(ValueError("Expected an argument"));
    }

    //
    // If no actor was specified and this is not a get operation, default to
    // the user store. Otherwise the action is performed on the override store,
    // causing no changes to stick, which is probably not what the user wanted.
    //

    if ((action != null) || (argc != 1)) {
        if (actor is SantaConfig) {
            actor = actor._user;
        }
    }

    if (argc == 1) {
        if (action == "remove") {
            actor.setKey(args[0], null);
            actor.save();

        } else if (action == null) {
            value = actor.getKey(args[0]);
            if ((value is Dict) || (value is List) || (json != false)) {
                Core.print(dumps(value, 4));

            } else {
                Core.print(value.__str());
            }

        } else {
            Core.raise(ValueError("Cannot perform %s with only one argument" %
                                  action));
        }

    } else if (argc == 2) {
        value = args[1];
        if (json != false) {
            value = loads(value);
        }

        if (action == "add") {
            previous = actor.getKey(args[0]);
            if (previous != null) {
                if (previous is List) {
                    previous.append(value);
                    value = previous;

                } else {
                    value = [previous, value];
                }
            }

            actor.setKey(args[0], value);
            actor.save();

        } else if (action == null) {
            actor.setKey(args[0], value);
            actor.save();

        } else {
            Core.raise(ValueError("Cannot perform %s with two arguments" %
                                  action));
        }

    //
    // Many arguments.
    //

    } else {
        value = [];
        if (action == "add") {
            previous = actor.getKey(args[0]);
            if (previous != null) {
                if (previous is List) {
                    value = previous;

                } else {
                    value = [previous];
                }
            }
        }

        for (arg in args[1...-1]) {
            if (json) {
                arg = loads(arg);
            }

            value.append(arg);
        }

        if ((action == null) || (action == "add")) {
            actor.setKey(args[0], value);
            actor.save();

        } else {
            Core.raise(ValueError("Cannot perform %s with many arguments" %
                                  action));
        }
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

function
_listConfig (
    actor,
    json
    )

/*++

Routine Description:

    This routine lists the configuration for the given dictionary.

Arguments:

    actor - Supplies the configuration to work on.

    json - Supplies whether or not to print in JSON format.

Return Value:

    Returns an exit code.

--*/

{

    actor = actor.dict();
    if (json) {
        Core.print(dumps(actor, 4));

    } else {
        _printConfigValue("", actor);
    }

    return 0;
}

function
_printConfigValue (
    prefix,
    value
    )

/*++

Routine Description:

    This routine prints a configuration value, recursively.

Arguments:

    prefix - Supplies the prefix to print on all values.

    value - Supplies the value to print.

Return Value:

    None.

--*/

{

    var index;

    if (value is Dict) {
        if (prefix != "") {
            prefix += ".";
        }

        for (key in value) {
            _printConfigValue("%s%s" % [prefix, key.__str()], value[key]);
        }

    } else if (value is List) {
        for (index in 0..value.length()) {
            _printConfigValue("%s[%d]" % [prefix, index], value[index]);
        }

    } else {
        Core.print("%s = %s" % [prefix, value.__str()]);
    }

    return;
}


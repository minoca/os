/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    santaconfig.ck

Abstract:

    This module contains the class that manages the global application
    configuration for the Santa package manager.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from iobase import IoError;
from os import dirname, getenv, mkdir, OsError, ENOENT, system, geteuid;
from santa.lib.config import ConfigFile;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// The path to load the system configuration from.
//

var SANTA_GLOBAL_CONFIG_PATH = "/etc/santa.conf";

//
// Define the suffix that goes on the end of the user's home directory to
// become Santa's home.
//

var SANTA_USER_SUFFIX = "/.santa";

//
// The path to load the user's configuration from.
//

var SANTA_USER_CONFIG_PATH = "~/santa.conf";

//
// Define the path to the patch manager's configuration.
//

var SANTA_PATCH_CONFIG_PATH = "~/patch.conf";

//
// Define the path to the package manager's state.
//

var SANTA_PACKAGE_STATE_PATH = "~/packages.json";

//
// Define the path to the package storage area.
//

var SANTA_STORAGE_PATH = "~/storage";

//
// Define the path to the default containers.
//

var SANTA_CONTAINER_PATH = "~/containers";

//
// Define the path to the realm configuration.
//

var SANTA_REALM_STATE_PATH = "~/realms.json";

//
// Define the root container path.
//

var SANTA_ROOT_CONTAINER_PATH = "/";
if (system == "Windows") {
    SANTA_ROOT_CONTAINER_PATH = SANTA_CONTAINER_PATH + "/root";
}

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// The default configuration.
//

var defaultConfig = {
    "core": {

        //
        // Define the prefix added to every file path opened by Santa.
        //

        "root": "",

        //
        // Set this to true to print more information about what's happening.
        //

        "verbose": false,

        //
        // Define the directory where global state is stored.
        //

        "statedir": "/var/lib/santa",

        //
        // Define locations where Santa extensions might be found.
        //

        "extensions": [
            "/usr/lib/santa/extensions",
            "~/extensions"
        ],

        //
        // Define the directory where packages are stored.
        //

        "pkgdir": "~/pkgs",
    },

    //
    // Define realm configuration.
    //

    "realm": {

        //
        // Set the configuration for the root realm.
        //

        "root": {
            "containment": {
                "type": "none",
                "path": SANTA_ROOT_CONTAINER_PATH
            },

            "presentation": {
                "type": "copy",
            },
        },

        //
        // Set the configuration for any new realm created.
        //

        "new": {
            "containment": {
                "type": (system == "Windows") ? "none" : "chroot",
            },

            "presentation": {
                "type": "copy",
            },
        }
    },

    //
    // Define repository locations. Null is a location so that the local
    // package directory is used (core.pkgdir).
    //

    "repositories": [
        null,
    ]
};

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

//
// This class manages the multitiered global application configuration.
//

class SantaConfig {
    var _global;
    var _user;
    var _override;

    function
    __init (
        configpath,
        override
        )

    /*++

    Routine Description:

        This routine initializes and loads the global Santa configuration.

    Arguments:

        configpath - Supplies an optional path of the config file to load. If
            null, then the default system and user configuration file paths
            will be loaded.

        override - Supplies a dictionary of configuration that should override
            the loaded values.

    Return Value:

        Returns the initialized instance.

    --*/

    {

        var directory;
        var home;
        var path;
        var root = "";
        var verbose = false;

        _override = ConfigFile(null, override);
        root = _override.getKey("core.root");
        if (!root) {
            root = "";
        }

        verbose = _override.getKey("core.verbose");

        //
        // Load a specific path only if requested.
        //

        if (configpath) {
            if (verbose) {
                Core.print("Loading direct config %s" % configpath);
            }

            _user = ConfigFile(configpath, {});
            _global = ConfigFile(null, {});
            return this;
        }

        path = root + SANTA_GLOBAL_CONFIG_PATH;
        _global = ConfigFile(path, defaultConfig);

        //
        // Try to create the configuration file if it doesn't exist.
        //

        if (_global._meta.loaded) {
            if (verbose) {
                Core.print("Loaded global config %s" % path);
            }

        } else {
            try {
                _global.save();
                if (verbose) {
                    Core.print("Saved defaults to %s" % path);
                }

            } except IoError {}
        }

        //
        // For the root user, set "home" to be the global directory, and don't
        // load a per-user configuration.
        //

        if (geteuid() == 0) {
            home = _global.getKey("core.statedir");
            _override.setKey("core.home", home);

        } else {

            //
            // Load up the user configuration file. If it starts with ~, prefix
            // with the user's home directory. If that cannot be found, don't
            // load anything.
            //

            path = SANTA_USER_CONFIG_PATH;
            if (path[0] == "~") {
                home = _override.getKey("core.home");
                if (!home) {
                    home = getenv("HOME");
                    if (!home) {
                        home = getenv("Home");
                        if (!home) {
                            home = getenv("HOMEPATH");
                            if (home) {
                                home = getenv("HOMEDRIVE") + home;
                            }
                        }
                    }

                    if (home) {
                        home += SANTA_USER_SUFFIX;
                    }
                }

                if (home) {
                    home = home.replace("\\", "/", -1);
                    _override.setKey("core.home", home);
                    path = root + home + path[1...-1];

                } else {
                    path = null;
                }

            } else {
                path = root + path;
            }
        }

        //
        // Attempt to load the user configuration.
        //

        if (path) {
            _user = ConfigFile(path, {});
            if (_user._meta.loaded) {
                if (verbose) {
                    Core.print("Loaded user config %s" % path);
                }

            } else {

                //
                // The user file couldn't be loaded. Try to create one.
                //

                try {
                    _user.save();
                    if (verbose) {
                        Core.print("Created config %s" % path);
                    }

                } except IoError as e {

                    //
                    // Create the directory if it doesn't exist.
                    //

                    if (e.errno == ENOENT) {
                        directory = dirname(path);
                        try {
                            mkdir(directory, 0700);
                            _user.save();
                            if (verbose) {
                                Core.print("Created config %s" % path);
                            }

                        } except [IoError, OsError] {}
                    }
                }
            }
        }

        return this;
    }

    function
    getKey (
        key
        )

    /*++

    Routine Description:

        This routine reads a config value, or returns null if it is not present.

    Arguments:

        key - Supplies the key to get, in "dot.notation".

    Return Value:

        Returns the value for the given key.

    --*/

    {

        var keys = key.split(".", -1);
        var value;

        for (dict in [_override, _user, _global]) {
            value = dict.getKey(key);
            if (value != null) {
                return value;
            }
        }

        return null;
    }

    function
    setKey (
        key,
        value
        )

    /*++

    Routine Description:

        This routine writes a config value to the override dictionary,
        creating the intermediate dictionaries if needed.

    Arguments:

        key - Supplies the key to get, in "dot.notation".

        value - Supplies the value to write. Supply null to remove a value.

    Return Value:

        None.

    --*/

    {

        //
        // If clearing something, clear it from all.
        //

        if (value == null) {
            for (dict in [_user, _global]) {
                dict.setKey(key, value);
            }
        }

        return _override.setKey(key, value);
    }

    //
    // Functions to make the object act like a dictionary.
    //

    function
    __get (
        key
        )

    /*++

    Routine Description:

        This routine performs a "get" operation, returning the value from the
        data dictionary.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the given key.

    --*/

    {

        if (key == "_global") {
            return _global;

        } else if (key == "_user") {
            return _user;

        } else if (key == "_override") {
            return _override;
        }

        //
        // Try to get a value from each of them, starting with the highest
        // precedence.
        //

        try {
            return _override.__get(key);

        } except KeyError {}

        try {
            return _user.__get(key);

        } except KeyError{}

        return _global.__get(key);
    }

    function
    __set (
        key,
        value
        )

    /*++

    Routine Description:

        This routine performs a "set" operation, saving a value into the data
        dictionary.

    Arguments:

        key - Supplies the key to set.

        value - Supplies the value to set.

    Return Value:

        None.

    --*/

    {

        return _override.__set(key, value);
    }

    function
    get (
        key
        )

    /*++

    Routine Description:

        This routine performs a "get" operation, returning the value from the
        data dictionary.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the given key.

        null if no value exists for that key.

    --*/

    {

        try {
            return this.__get(key);

        } except KeyError {}

        return null;
    }

    function
    set (
        key,
        value
        )

    /*++

    Routine Description:

        This routine performs a "set" operation, saving a value into the data
        dictionary.

    Arguments:

        key - Supplies the key to set.

        value - Supplies the value to set.

    Return Value:

        None.

    --*/

    {

        return this.__set(key, value);
    }

    function
    __slice (
        key
        )

    /*++

    Routine Description:

        This routine executes the slice operator, which is called when square
        brackets are used.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the key in the data dictionary.

    --*/

    {

        return this.__get(key);
    }

    function
    __sliceAssign (
        key,
        value
        )

    /*++

    Routine Description:

        This routine executes the slice assignment operator, which is called
        when square brackets are used on the left side of an assignment.

    Arguments:

        key - Supplies the key to get.

    Return Value:

        Returns the value for the key in the data dictionary.

    --*/

    {

        return this.__set(key);
    }

    function
    __str (
        )

    /*++

    Routine Description:

        This routine converts the object into a string.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        return "<SantaConfig>";
    }

    function
    dict (
        )

    /*++

    Routine Description:

        This routine creates a merged dictionary of all config options.

    Arguments:

        None.

    Return Value:

        Returns a new dictionary containing all of the options combined.

    --*/

    {

        var result = {};

        this._mergedicts(result, _global);
        this._mergedicts(result, _user);
        this._mergedicts(result, _override);
        return result;
    }

    function
    _mergedicts (
        dest,
        source
        )

    /*++

    Routine Description:

        This routine merges one dictionary into another, recursively. Values
        other than dictionaries are smashed from the source onto the
        destination.

    Arguments:

        dest - Supplies the destination dictionary.

        source - Supplies the source dictionary to merge with.

    Return Value:

        Returns a new dictionary containing all of the keys combined.

    --*/

    {

        var destvalue;
        var sourcevalue;

        for (key in source) {
            sourcevalue = source[key];
            if (sourcevalue is Dict) {
                destvalue = dest.get(key);
                if (destvalue is Dict) {
                    dest[key] = this._mergedicts(destvalue, sourcevalue);

                } else {
                    dest[key] = sourcevalue.copy();
                }

            } else {
                dest[key] = sourcevalue;
            }
        }

        return dest;
    }

    function
    keys (
        )

    /*++

    Routine Description:

        This routine returns the keys in the data dictionary.

    Arguments:

        None.

    Return Value:

        Returns a list of keys in the dictionary.

    --*/

    {

        var dictkeys;
        var keydict = {};

        //
        // Combine the unique elements of each dictionary into a single set.
        //

        dictkeys = _override.keys();
        for (key in dictkeys) {
            keydict[key] = true;
        }

        dictkeys = _user.keys();
        for (key in dictkeys) {
            keydict[key] = true;
        }

        dictkeys = _global.keys();
        for (key in dictkeys) {
            keydict[key] = true;
        }

        return keydict.keys();
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine saves all configuration information to disk.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        _user.save();
        _global.save();
        return;
    }
}


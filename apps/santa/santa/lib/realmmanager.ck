/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realmanager.ck

Abstract:

    This module implements support for managing Realms.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.config import config;
from santa.lib.config import ConfigFile;
from santa.file import open;
from santa.lib.realm import Realm;

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

var defaultRealmsConfig = {
    "realms": {}
};

//
// Define the singleton instance.
//

var _singleton;

//
// ------------------------------------------------------------------ Functions
//

class RealmManager {
    var _config;

    function
    __init (
        )

    /*++

    Routine Description:

        This routine initializes an unconfigured realm.

    Arguments:

        None.

    Return Value:

        Returns an exit code.

    --*/

    {

        var configPath;

        //
        // Return the singleton if it's already been initialized.
        //

        if (_singleton is RealmManager) {
            return _singleton;
        }

        configPath = "/".join([config.getKey("core.statedir"), "realm.json"]);
        _config = ConfigFile(configPath, defaultRealmsConfig);
        _singleton = this;
        return this;
    }

    function
    getRootRealm (
        )

    /*++

    Routine Description:

        This routine returns the root realm.

    Arguments:

        name - Supplies the name of the realm to find.

    Return Value:

        Returns the realm on success.

        Raises a KeyError exception if no realm with the given name exists.

    --*/

    {

        var realm;
        var realmConfig = config.getKey("realm.root");

        realm = Realm();
        realm.load(realmConfig);
        realm.name = "root";
        return realm;
    }

    function
    getRealm (
        name
        )

    /*++

    Routine Description:

        This routine returns the realm with the given name.

    Arguments:

        name - Supplies the name of the realm to find.

    Return Value:

        Returns the realm on success.

        Raises a KeyError exception if no realm with the given name exists.

    --*/

    {

        var realm;
        var realmConfig;

        if (name == "root") {
            return this.getRootRealm();
        }

        realmConfig = _config.getKey("realms." + name);
        if (realmConfig == null) {
            Core.raise(KeyError("Realm with name '%s' does not exist" % name));
        }

        realm = Realm();
        realm.load(realmConfig);
        realm.name = name;
        return realm;
    }

    function
    createRealm (
        name,
        parameters
        )

    /*++

    Routine Description:

        This routine creates a new realm.

    Arguments:

        name - Supplies the name of the new realm.

        parameters - Supplies additional parameters to the realm creation.

    Return Value:

        Returns the new realm on success.

    --*/

    {

        var data;
        var realm;
        var realmConfig = config.getKey("realm.new");

        if (!(realmConfig is Dict)) {
            realmConfig = parameters;

        } else {
            realmConfig = realmConfig.copy();
            for (key in parameters) {
                realmConfig[key] = parameters[key];
            }
        }

        try {
            this.getRealm(name);
            Core.raise(ValueError("Realm with name '%s' already exists" %
                                  name));

        } except KeyError {}

        realm = Realm();
        realm.create(realmConfig);
        realm.name = name;
        data = realm.save();
        _config.setKey("realms." + name, data);
        _config.save();
        return realm;
    }

    function
    destroyRealm (
        name
        )

    /*++

    Routine Description:

        This routine destroys a realm with the given name.

    Arguments:

        name - Supplies the name of the realm to remove.

    Return Value:

        None.

    --*/

    {

        var realm = this.getRealm(name);
        var realms = _config.realms;

        if (name == "root") {
            Core.raise(ValueError("Root realm cannot be destroyed"));
        }

        realm.destroy();
        realms.remove(name);
        _config.save();
        return;
    }

    function
    enumerateRealms (
        )

    /*++

    Routine Description:

        This routine returns a list of realms by name.

    Arguments:

        None.

    Return Value:

        Returns the list of active realm names on success.

    --*/

    {

        var names;
        var realms = _config.getKey("realms");

        if (!(realms is Dict)) {
            return ["root"];
        }

        return ["root"] + realms.keys();
    }
}

function
getRealmManager (
    )

/*++

Routine Description:

    This routine returns the singleton realm manager.

Arguments:

    None.

Return Value:

    Returns the realm manager.

--*/

{

    if (_singleton is RealmManager) {
        return _singleton;
    }

    _singleton = RealmManager();
    return _singleton;
}

//
// --------------------------------------------------------- Internal Functions
//


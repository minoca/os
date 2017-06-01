/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realm.ck

Abstract:

    This module implements support for realms within Santa.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.file import cptree, link, rmtree;
from santa.lib.santaconfig import SANTA_GLOBAL_CONFIG_PATH;
from santa.modules import getContainment, getStorage, getPresentation;

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

//
// ------------------------------------------------------------------ Functions
//

class Realm {
    var _containment;
    var _storage;
    var _presentation;
    var _parameters;

    function
    create (
        parameters
        )

    /*++

    Routine Description:

        This routine creates a new Realm.

    Arguments:

        parameters - Supplies the realm creation parameters.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        var type;

        _parameters = parameters;

        //
        // Create the container, storage, and presentation objects based on
        // their configured types.
        //

        type = getContainment(_parameters.containment.type);
        _containment = type();
        _containment.create(parameters.containment);
        type = getStorage(_parameters.storage.type);
        _storage = type();
        _storage.create(parameters.storage);
        type = getPresentation(parameters.presentation.type);
        _presentation = type();
        _presentation.create(parameters.presentation);
        this.setupSharing(false);
        return null;
    }

    function
    destroy (
        )

    /*++

    Routine Description:

        This routine removes a realm from disk.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        this.setupSharing(true);
        _presentation.destroy();
        _presentation = null;
        _storage.destroy();
        _storage = null;
        _containment.destroy();
        _containment = null;
        _parameters = null;
        return;
    }

    function
    load (
        parameters
        )

    /*++

    Routine Description:

        This routine loads a Realm from previously saved parameters.

    Arguments:

        parameters - Supplies the realm's saved parameters.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        var type;

        _parameters = parameters;

        //
        // Create the container, storage, and presentation objects based on
        // their configured types.
        //

        type = getContainment(_parameters.containment.type);
        _containment = type();
        _containment.load(parameters.containment);
        type = getStorage(_parameters.storage.type);
        _storage = type();
        _storage.load(parameters.storage);
        type = getPresentation(parameters.presentation.type);
        _presentation = type();
        _presentation.load(parameters.presentation);
        return;
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine returns a dictionary representing the realm's current
        state.

    Arguments:

        None.

    Return Value:

        Returns the current dictionary of the realm's state on success.

        Raises an exception on failure.

    --*/

    {

        _parameters.containment = _containment.save();
        _parameters.storage = _storage.save();
        _parameters.presentation = _presentation.save();
        return _parameters;
    }

    function
    setupSharing (
        tearDown
        )

    /*++

    Routine Description:

        This routine sets up shared data between the parent and this new
        child realm.

    Arguments:

        tearDown - Supplies a boolean indicating whether or not to actually
            tear down sharing rather than setting it up.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        var destination;
        var method;
        var sharing;
        var source;

        sharing = _parameters.get("sharing");
        if (sharing is Dict) {
            for (element in sharing) {
                method = sharing[element];
                if (element == "store") {
                    source = _storage.path("/", true);
                    if (source == "/") {
                        source = null;
                    }

                    destination = source;

                } else if (element == "globalconfig") {
                    source = SANTA_GLOBAL_CONFIG_PATH;
                    destination = source;

                } else if (element[0] == "/") {
                    source = element;
                    destination = element;

                } else {
                    Core.raise(ValueError("Unknown shareable %s" % element));
                }

                if (tearDown) {
                    if (destination) {
                        this._unshareFile(destination, method);
                    }

                } else {
                    if (source) {
                        this._shareFile(source, destination, method);
                    }
                }
            }
        }

        return;
    }

    function
    _shareFile (
        source,
        destination,
        method
        )

    /*++

    Routine Description:

        This routine creates a shared file.

    Arguments:

        source - Supplies the source path to share with the child realm.

        destination - Supplies the relative destination within the child to
            share the source at.

        method - Supplies the sharing method.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        destination = _storage.path(destination, true);
        destination = _containment.path(destination);
        if (method == "hardlink") {
            link(source, destination);

        } else if (method == "copy") {
            cptree(source, destination);

        } else if (method != "none") {
            Core.raise(ValueError("Unknown share method '%s'" % method));
        }

        return;
    }

    function
    _unshareFile (
        destination,
        method
        )

    /*++

    Routine Description:

        This routine removes a shared file.

    Arguments:

        destination - Supplies the relative destination within the child where
            the file was shared to.

        method - Supplies the sharing method.

    Return Value:

        Null on success.

        Raises an exception on failure.

    --*/

    {

        destination = _storage.path(destination, true);
        destination = _containment.path(destination);
        if ((method == "hardlink") || (method == "copy")) {
            rmtree(destination);

        } else {
            Core.raise(ValueError("Unknown share method '%s'" % method));
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//


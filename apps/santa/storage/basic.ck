/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    basic.ck

Abstract:

    This module implements basic storage support, basically a directory.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.config import config;
from santa.storage import Storage, StorageError;
from santa.file import mkdir, rmtree;

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

class BasicStorage is Storage {
    var _parameters;

    function
    create (
        parameters
        )

    /*++

    Routine Description:

        This routine creates a new storage region and initializes this
        instance's internal variables to represent it.

    Arguments:

        parameters - Supplies a dictionary of creation parameters.

    Return Value:

        None.

    --*/

    {

        var path;

        _parameters = parameters.copy();
        _parameters.type = "basic";
        try {
            path = parameters.path;

        } except KeyError {
            Core.raise(StorageError("Required parameter is missing"));
        }

        if ((path == "") || (path == "/")) {
            Core.raise(StorageError("Invalid basic storage path"));
        }

        mkdir(path);
        if (config.getKey("core.verbose")) {
            Core.print("Created basic storage region at %s" % path);
        }

        return;
    }

    function
    destroy (
        )

    /*++

    Routine Description:

        This routine destroys a storage zone and all data held therein.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var path = _parameters.path;
        var verbose = config.getKey("core.verbose");

        if (verbose) {
            Core.print("Destroying storage region at %s" % path);
        }

        rmtree(path);
        _parameters = null;
        if (verbose) {
            Core.print("Destroyed storage region at %s" % path);
        }

        return;
    }

    function
    load (
        parameters
        )

    /*++

    Routine Description:

        This routine initializes this instance to reflect the storage zone
        identified by the given parameters.

    Arguments:

        parameters - Supplies a dictionary of parameters.

    Return Value:

        None.

    --*/

    {

        _parameters = parameters.copy();

        try {
            parameters.path;

        } except KeyError {
            Core.raise(ContainmentError("Required parameter is missing"));
        }

        return;
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine returns the dictionary of state and identification needed
        to restore information about this storage zone by other instances of
        this class.

    Arguments:

        None.

    Return Value:

        Returns a dictionary of parameters to save describing this instance.

    --*/

    {

        return _parameters;
    }

    function
    path (
        filepath,
        write
        )

    /*++

    Routine Description:

        This routine translates a path to a path within the storage zone.

    Arguments:

        filepath - Supplies a path to the desired file.

        write - Supplies a boolean indicating whether or not write access is
            needed. This should only be used internally to extract the packages,
            and should not be used to allow modifications to the package file.

    Return Value:

        Returns the path to the file within the storage region.

    --*/

    {

        //
        // Basic paths are the same regardless of whether the caller wants
        // read or write access.
        //

        return "/".join([_parameters.path, filepath]);
    }
}

//
// Set the variables needed so that the module loader can enumerate this class.
//

var description = "Basic package storage in a directory.";
var storage = BasicStorage;

//
// --------------------------------------------------------- Internal Functions
//


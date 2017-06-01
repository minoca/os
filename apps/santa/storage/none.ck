/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    none.ck

Abstract:

    This module implements no-op storage, where packages contents are simply
    stored at their final location.

Author:

    Evan Green 1-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.config import config;
from santa.storage import Storage, StorageError;

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

class NoneStorage is Storage {
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
        _parameters.type = "none";
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

        _parameters = null;
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

        _parameters = parameters;
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

        return filepath;
    }
}

//
// Set the variables needed so that the module loader can enumerate this class.
//

var description = "No package contents storage, other than final location";
var storage = NoneStorage;

//
// --------------------------------------------------------- Internal Functions
//


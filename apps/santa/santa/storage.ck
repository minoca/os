/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    storage.ck

Abstract:

    This module implements generic package storage support.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

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

class StorageError is Exception {}

class Storage {

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

        Core.raise(StorageError("Function not implemented"));
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

        Core.raise(StorageError("Function not implemented"));
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

        Core.raise(StorageError("Function not implemented"));
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

        Core.raise(StorageError("Function not implemented"));
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

        Core.raise(StorageError("Function not implemented"));
    }

}

//
// --------------------------------------------------------- Internal Functions
//


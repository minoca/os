/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    containment.ck

Abstract:

    This module implements generic containment support.

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

class ContainmentError is Exception {}

class Containment {

    function
    create (
        parameters
        )

    /*++

    Routine Description:

        This routine creates a new container and initializes this instance's
        internal variables to represent it.

    Arguments:

        parameters - Supplies a dictionary of creation parameters.

    Return Value:

        None.

    --*/

    {

        Core.raise(ContainmentError("Function not implemented"));
    }

    function
    destroy (
        )

    /*++

    Routine Description:

        This routine destroys the container represented by this instance.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        Core.raise(ContainmentError("Function not implemented"));
    }

    function
    load (
        parameters
        )

    /*++

    Routine Description:

        This routine initializes this instance to reflect the container
        identified by the given parameters.

    Arguments:

        parameters - Supplies a dictionary of parameters.

    Return Value:

        None.

    --*/

    {

        Core.raise(ContainmentError("Function not implemented"));
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine returns the dictionary of state and identification needed
        to restore information about this container by other instances of this
        class.

    Arguments:

        None.

    Return Value:

        Returns a dictionary of parameters to save describing this instance.

    --*/

    {

        Core.raise(ContainmentError("Function not implemented"));
    }

    function
    enter (
        parameters
        )

    /*++

    Routine Description:

        This routine enters the given container environment.

    Arguments:

        parameters - Supplies an optional set of additional parameters
            specific to this entry into the environment.

    Return Value:

        None. Upon return, the current execution environment will be confined
        to the container.

    --*/

    {

        Core.raise(ContainmentError("Function not implemented"));
    }

    function
    path (
        filepath
        )

    /*++

    Routine Description:

        This routine translates from a path within the container to a path
        outside of the container.

    Arguments:

        filepath - Supplies the path rooted from within the container.

    Return Value:

        Returns the path to the file from the perspective of an application
        not executing within the container.

    --*/

    {

        Core.raise(ContainmentError("Function not implemented"));
    }

}

//
// --------------------------------------------------------- Internal Functions
//


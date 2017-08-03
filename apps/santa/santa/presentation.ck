/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    presentation.ck

Abstract:

    This module implements generic package presentation support.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import os;
from santa.file import rmtree;

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

class PresentationError is Exception {}

class Presentation {

    function
    create (
        parameters
        )

    /*++

    Routine Description:

        This routine creates a new presentation layer and initializes this
        instance's internal variables to represent it.

    Arguments:

        parameters - Supplies a dictionary of creation parameters.

    Return Value:

        None.

    --*/

    {

        Core.raise(PresentationError("Function not implemented"));
    }

    function
    destroy (
        )

    /*++

    Routine Description:

        This routine destroys the presentation layer represented by this
        instance.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        Core.raise(PresentationError("Function not implemented"));
    }

    function
    load (
        parameters
        )

    /*++

    Routine Description:

        This routine initializes this instance to reflect the presentation
        identified by the given parameters.

    Arguments:

        parameters - Supplies a dictionary of parameters.

    Return Value:

        None.

    --*/

    {

        Core.raise(PresentationError("Function not implemented"));
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine returns the dictionary of state and identification needed
        to restore information about this presentation layer by other instances
        of this class.

    Arguments:

        None.

    Return Value:

        Returns a dictionary of parameters to save describing this instance.

    --*/

    {

        Core.raise(PresentationError("Function not implemented"));
    }

    function
    addFiles (
        controlDirectory,
        realm,
        files,
        conffiles,
        root
        )

    /*++

    Routine Description:

        This routine adds a set of files into the environment.

    Arguments:

        controlDirectory - Supplies the directory containing the control and
            initial data of the package.

        realm - Supplies the realm being operated on.

        files - Supplies the files to add.

        conffiles - Suppiles the dictionary of files not to clobber if they
            exist, or to copy directly if they do not.

        root - Supplies the root directory to install to.

    Return Value:

        None.

    --*/

    {

        Core.raise(PresentationError("Function not implemented"));
    }

    function
    removeFiles (
        controlDirectory,
        realm,
        files,
        conffiles,
        root
        )

    /*++

    Routine Description:

        This routine removes a set of files from the environment.

    Arguments:

        controlDirectory - Supplies the directory containing the control and
            initial data of the package.

        realm - Supplies the realm being operated on.

        files - Supplies the files to remove.

        conffiles - Suppiles the dictionary of files not to remove if they've
            changed.

        root - Supplies the root directory to install to.

    Return Value:

        None.

    --*/

    {

        var destdir = realm.containment.outerPath(root);
        var dirs = [];
        var filepath;

        //
        // The default implementation just removes the files. Start with the
        // files themselves.
        //

        for (file in files) {
            filepath = "/".join([destdir, file]);
            if ((os.isdir)(filepath)) {
                dirs.append(filepath);

            } else {
                rmtree(filepath);
            }
        }

        //
        // Now try to remove the directories, ignoring failure if they're not
        // empty.
        //

        for (dir in dirs) {
            try {
                (os.rmdir)(dir);

            } except os.OsError as e {
                if (e.errno != os.ENOTEMPTY) {
                    Core.raise(e);
                }
            }
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//


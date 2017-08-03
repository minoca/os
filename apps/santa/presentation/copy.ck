/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    copy.ck

Abstract:

    This module implements package presentation based on copying files from
    storage into their final destination.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.config import config;
from santa.file import cptree, exists, mkdir, isdir;
from santa.presentation import Presentation, PresentationError;

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

class CopyPresentation is Presentation {
    var _parameters;

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

        _parameters = parameters.copy();
        this.type = "copy";
        return;
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

        return;
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

        _parameters = parameters;
        this.type = "copy";
        return;
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

        return _parameters;
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

        var dest;
        var destdir = realm.containment.outerPath(root);
        var srcfile;
        var srcdir = controlDirectory + "/data";

        if (destdir.endsWith("/")) {
            destdir = destdir[0..-1];
        }

        //
        // First create all the directories.
        //

        for (file in files) {
            srcfile = "/".join([srcdir, file]);
            if (isdir(srcfile)) {
                mkdir("/".join([destdir, file]));
            }
        }

        //
        // Now copy all the files.
        //

        for (file in files) {
            srcfile = "/".join([srcdir, file]);
            if (isdir(srcfile)) {
                continue;
            }

            dest = "/".join([destdir, file]);
            if ((conffiles.get(dest) != null) && (exists(dest))) {
                if (config.getKey("core.verbose")) {
                    Core.print("Skipping pre-existing configuration file: %s" %
                               dest);
                }

                continue;
            }

            cptree(srcfile, dest);
        }

        return;
    }
}

//
// Define the globals needed so the module loader can pick up this class.
//

var description = "Copy-based installation";
var presentation = CopyPresentation;

//
// --------------------------------------------------------- Internal Functions
//


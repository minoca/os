/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chroot.ck

Abstract:

    This module implements chroot-based container support.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from santa.build import shell;
from santa.config import config;
from santa.containment import Containment, ContainmentError;
from santa.file import chdir, chmod, chroot, mkdir, path, rmtree;

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

class ChrootContainment is Containment {
    var _parameters;

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

        var rootpath;

        _parameters = parameters.copy();
        _parameters.type = "chroot";
        try {
            rootpath = parameters.path;

        } except KeyError {
            Core.raise(ContainmentError("Required parameter is missing"));
        }

        if ((rootpath == "") || (rootpath == "/")) {
            Core.raise(ContainmentError("Invalid chroot path"));
        }

        mkdir(rootpath + "/tmp");
        chmod(rootpath + "/tmp", 0777);
        mkdir(rootpath + "/dev");
        shell("set -e\n"
              "_world=%s\n"
              "for d in null full zero urandom tty; do\n"
              "  touch $_world/dev/$d\n"
              "  mount --bind /dev/$d $_world/dev/$d\n"
              "done" % path(rootpath));

        if (config.getKey("core.verbose")) {
            Core.print("Created chroot environment at %s" % rootpath);
        }

        return;
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

        var rootpath = _parameters.path;
        var verbose = config.getKey("core.verbose");

        if (verbose) {
            Core.print("Destroying chroot environment at %s" % rootpath);
        }

        shell("_world=%s\n"
              "for d in null full zero urandom tty; do\n"
              "  umount $_world/dev/$d\n"
              "done\n"
              "true" % path(rootpath));

        rmtree(rootpath);
        _parameters = null;
        if (verbose) {
            Core.print("Destroyed chroot environment at %s" % rootpath);
        }

        return;
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

        _parameters = parameters;

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
        to restore information about this container by other instances of this
        class.

    Arguments:

        None.

    Return Value:

        Returns a dictionary of parameters to save describing this instance.

    --*/

    {

        return _parameters;
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

        var path = _parameters.path;

        chdir(path);
        chroot(path);
        if (config.getKey("core.verbose")) {
            Core.print("Entered chroot environment at %s" % path);
        }
    }

    function
    outerPath (
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

        var rootpath = path(_parameters.path);

        if (filepath == "/") {
            return rootpath;
        }

        return "/".join([rootpath, filepath]);
    }

    function
    innerPath (
        filepath
        )

    /*++

    Routine Description:

        This routine translates from a path outside the container to a path
        within of the container.

    Arguments:

        filepath - Supplies the path rooted from outside the container.

    Return Value:

        Returns the path to the file from the perspective of an application
        executing within the container.

    --*/

    {

        var rootpath = _parameters.path;

        if (!filepath.startsWith(rootpath)) {
            rootpath = path(rootpath);
            if (!filepath.startsWith(rootpath)) {
                Core.raise(ValueError("Path '%s' does not start in container "
                                      "rooted at '%s'" % [filepath, rootpath]));
            }
        }

        filepath = filepath[rootpath.length()...-1];
        if (filepath == "") {
            return "/";
        }

        return filepath;
    }
}

//
// Set the variables needed so that the module loader can enumerate this class.
//

var description = "Containment based on chroot.";
var containment = ChrootContainment;

//
// --------------------------------------------------------- Internal Functions
//


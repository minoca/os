/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pkgdepot.ck

Abstract:

    This module implements the PackageDepot class, which manages a collection
    of packages in a directory.

Author:

    Evan Green 13-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

from bufferedio import BufferedIo;
import cpio;
import io;
from iobase import IoError;
import json;
import lzfile;
import os;
from santa.config import config;
from santa.file import path, mkdir;
from santa.lib.archive import Archive;
import santa.lib.pkgbuilder;

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

class PackageDepot {
    var _directory;

    function
    __init (
        directory
        )

    /*++

    Routine Description:

        This routine initializes a package repository instance.

    Arguments:

        directory - Supplies the directory where the package repository lives.
            This directory will be created if it does not exist.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        if (directory == null) {
            directory = config.getKey("core.pkgdir");
            if (directory == null) {
                Core.raise(ValueError("Error: core.pkgdir not set in config"));
            }
        }

        mkdir(directory);
        _directory = directory;
        this.pkgs = null;
        this.location = directory;
        return this;
    }

    function
    load (
        force
        )

    /*++

    Routine Description:

        This routine loads the package index.

    Arguments:

        force - Supplies a boolean indicating whether to force reload or not.
            If not, and the index is already loaded, it is not loaded again.

    Return Value:

        None.

    --*/

    {

        if (force) {
            this.pkgs = null;
        }

        return this._load();
    }

    function
    createPackage (
        vars,
        dataDirectory,
        updateIndex
        )

    /*++

    Routine Description:

        This routine creates a new package in the depot from a set of completed
        build variables.

    Arguments:

        vars - Supplies the dictionary of variables needed to assemble the
            package.

        dataDirectory - Supplies the directory containing the package's
            contents.

        updateIndex - Supplies a boolean indicating whether or not to update
            the package index.

    Return Value:

        None.

    --*/

    {

        var directory;
        var package = (pkgbuilder.PackageBuilder)(vars, dataDirectory);
        var section = vars.section;

        directory = "%s/%s-%s/%s" %
                    [_directory, vars.os, vars.arch, section];

        mkdir(directory);
        package.build(path(directory));
        if (updateIndex) {
            this.rebuildIndex();
        }

        return;
    }

    function
    rebuildIndex (
        )

    /*++

    Routine Description:

        This routine rebuilds the package index.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var directory = path(_directory);
        var index = (pkgbuilder.PackageIndexBuilder)(directory);

        index.build(directory + "/index.json", null, null, "warn");
        if (this.pkgs != null) {
            this.load(true);
        }

        return;
    }

    function
    _load (
        )

    /*++

    Routine Description:

        This routine loads the package index if it has not already been loaded.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var data;
        var file;
        var indexPath;

        if (this.pkgs != null) {
            return;
        }

        indexPath = path(_directory) + "/index.json";
        try {
            file = (lzfile.LzFile)(indexPath, "r", 9);
            file = BufferedIo(file, 0);
            data = file.readall();
            file.close();
            this.pkgs = (json.loads)(data);

        } except IoError as e {
            if (e.errno == os.ENOENT) {
                this.pkgs = [];
                return;
            }

            Core.raise(e);
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pkg.ck

Abstract:

    This module implements the Package and InstalledPackage classes, which
    encapsulate a single package.

Author:

    Evan Green 14-Jul-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import cpio;
import json;
import os;
from santa.config import config;
from santa.file import rmtree;
from santa.lib.archive import Archive;
from santa.lib.config import ConfigFile;

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

var packageDefaultStatus = {
    "status": {
        "extracted": false,
        "files": []
    },

    "archive": null,
    "info": {}
};

//
// ------------------------------------------------------------------ Functions
//

class Package {
    var _module;
    var _label;
    var _conffiles;
    var _config;
    var _controlDirectory;

    static
    function
    fromControlDirectory (
        controlDirectory
        )

    /*++

    Routine Description:

        This routine loads a previously installed Package.

    Arguments:

        controlDirectory - Supplies the control directory of the package.

    Return Value:

        Returns a new Package instance.

    --*/

    {

        return Package(controlDirectory, null);
    }

    static
    function
    fromArchive (
        controlDirectory,
        path
        )

    /*++

    Routine Description:

        This routine loads a package from its file path.

    Arguments:

        controlDirectory - Supplies the location of the control directory.

        path - Supplies the path of the package file.

    Return Value:

        Returns a new Package instance.

    --*/

    {

        return Package(controlDirectory, path);
    }

    function
    __init (
        controlDirectory,
        path
        )

    /*++

    Routine Description:

        This routine instantiates a new Package.

    Arguments:

        controlDirectory - Supplies the location of the control directory.

        path - Supplies an optional path to the package file itself. This is
            only valid for packages that have not yet been installed.

    Return Value:

        Returns the initialized instance.

    --*/

    {

        var archive;
        var member;
        var memberFile;

        _controlDirectory = controlDirectory;
        _config = ConfigFile(controlDirectory + "/status.json",
                             packageDefaultStatus);

        this.status = _config.status;
        this.info = _config.info;
        this.archive = _config.archive;
        if ((this.info.length() == 0) && (path != null)) {

            //
            // Save the path and extract the info initially.
            //

            this.archive = path;
            archive = (cpio.CpioArchive)(path, "r");
            member = archive.getMember("info.json");
            memberFile = archive.extractFile(member);
            this.info = (json.loads)(memberFile.readall());
            memberFile.close();
            archive.close();
        }

        _label = "%s-%s" % [this.info.name, this.info.version];
        _conffiles = {};
        for (file in this.info.conffiles) {
            _conffiles[file] = true;
        }

        return this;
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine returns a dictionary containing the state for the
        instance, which can be reloaded later.

    Arguments:

        None.

    Return Value:

        Returns a state dictionary that can be passed to the __init routine.

    --*/

    {

        _config.status = this.status;
        _config.info = this.info;
        _config.archive = this.archive;
        _config.save();
        return;
    }

    function
    install (
        realm,
        root
        )

    /*++

    Routine Description:

        This routine installs a package.

    Arguments:

        realm - Supplies the realm to install the package in.

        root - Supplies the alternate path to install the package in. Supply
            null to install it in the default "/".

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        this._extract();
        this._loadModule();
        this._runInstallScript("preinstall");
        if (realm.presentation.type == "move") {
            this.status.extracted = false;
        }

        if ((root == null) || (root == "")) {
            root = "/";
        }

        realm.presentation.addFiles(_controlDirectory,
                                    realm,
                                    this.status.files,
                                    _conffiles,
                                    root);

        this._runInstallScript("postinstall");
        return;
    }

    function
    uninstall (
        realm,
        root
        )

    /*++

    Routine Description:

        This routine uninstalls a package.

    Arguments:

        controlDirectory - Supplies the path where the control files are
            located.

        realm - Supplies the realm to uninstall the package from.

        root - Supplies the alternate path to uninstall the package from.
            Supply null to uninstall from the default "/".

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        this._loadModule();
        this._runInstallScript("preremove");
        realm.presentation.removeFiles(_controlDirectory,
                                       realm,
                                       this.status.files,
                                       _conffiles,
                                       root);

        this._runInstallScript("postremove");
        return;
    }

    function
    remove (
        )

    /*++

    Routine Description:

        This routine removes the package control directory and all of its
        contents.

    Arguments:

        None.

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        rmtree(_controlDirectory);
        return;
    }

    function
    _extract (
        )

    /*++

    Routine Description:

        This routine installs a package.

    Arguments:

        None.

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        var archive;
        var archiveTime;
        var files = [];
        var dataArchivePath;
        var dataDirectory;
        var statusTime;

        //
        // If the package is already extracted, do a quick check to see if
        // the archive is newer, and re-extract if so.
        //

        if (this.status.extracted != false) {
            if (this.archive == null) {
                return;
            }

            try {
                archiveTime = (os.stat)(this.archive).st_mtime;
                statusTime =
                        (os.stat)(_controlDirectory + "/status.json").st_mtime;

                if (archiveTime > statusTime) {
                    if (config.getKey("core.verbose")) {
                        Core.print("Re-extracting, as archive %s time %d "
                                   "newer than status time %d" %
                                   [this.archive, archiveTime, statusTime]);
                    }

                    this.status.extracted = false;
                }

            } except os.OsError {}

            if (this.status.extracted != false) {
                return;
            }
        }

        if (this.archive == null) {
            Core.raise(ValueError("%s: Missing path" % _label));
        }

        //
        // Extract the contents of the archive in the control directory.
        //

        archive = (cpio.CpioArchive)(this.archive, "r");
        for (member in archive) {
            if (member.name == "info.json") {
                continue;
            }

            archive.extract(member, _controlDirectory, -1, -1, false);
        }

        archive.close();

        //
        // Open up the data archive.
        //

        dataArchivePath = _controlDirectory + "/data.cpio.lz";
        dataDirectory = _controlDirectory + "/data";
        try {
            (os.mkdir)(dataDirectory, 0775);

        } except os.OsError as e {
            if (e.errno != os.EEXIST) {
                Core.raise(e);
            }
        }

        archive = Archive.open(dataArchivePath, "r");
        for (member in archive) {
            if (member.name.startsWith("/")) {
                Core.raise(ValueError("%s: Archive data should be relative "
                                      "paths, got: %s" %
                                      [_label, member.name]));
            }

            files.append(member.name);
            archive.extractMember(member, dataDirectory, -1, -1, true);
        }

        archive.close();
        (os.unlink)(dataArchivePath);
        this.status.extracted = true;
        this.status.files = files;
        return;
    }

    function
    _loadModule (
        )

    /*++

    Routine Description:

        This routine loads the module containing installation scripts for the
        package.

    Arguments:

        None.

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        var modulePath;

        if (_module) {
            return;
        }

        if (!(os.exists)(_controlDirectory + "/install.ck")) {
            return;
        }

        modulePath = Core.modulePath();
        Core.setModulePath([_controlDirectory] + modulePath);
        _module = Core.importModule("install");
        _module.run();

        //
        // Restore the original module path.
        //

        Core.setModulePath(modulePath);
        return;
    }

    function
    _runInstallScript (
        name
        )

    /*++

    Routine Description:

        This routine runs an installation function in the install script
        provided with the package.

    Arguments:

        name - Supplies the name of the function run within the install script.

    Return Value:

        None. An exception is raised on error.

    --*/

    {

        var installFunction;
        var verbose = config.getKey("core.verbose");

        if (_module == null) {
            return;
        }

        try {
            installFunction = _module.__get(name);

        } except NameError {
            return;
        }

        if (verbose) {
            Core.print("%s: Running %s" % [_label, name]);
        }

        installFunction();
        if (verbose) {
            Core.print("%s: Completed %s" % [_label, name]);
        }

        return;
    }
}

//
// --------------------------------------------------------- Internal Functions
//


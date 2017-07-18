/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pkgbuilder.ck

Abstract:

    This module implements the PackageBuilder class, which assembles packages.

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
import json;
import lzfile;
import os;
from santa.lib.archive import Archive;

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

class PackageIndexError is Exception {}

class PackageBuilder {
    function
    __init (
        vars,
        dataDirectory
        )

    /*++

    Routine Description:

        This routine creates a new package builder instance from a set of
        variables.

    Arguments:

        vars - Supplies the dictionary of variables needed to assemble the
            package.

        dataDirectory - Supplies the directory containing the package's
            contents.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        this.vars = vars;
        this.datadir = dataDirectory;
        return this;
    }

    function
    build (
        outputDirectory
        )

    /*++

    Routine Description:

        This routine builds a package from the current instance.

    Arguments:

        outputDirectory - Supplies an optional pointer to use for the output
            directory.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        var archive;
        var file;
        var finalPath;
        var installScript;
        var contents;
        var info;
        var stats;
        var vars = this.vars;

        if ((vars.arch == "all") || (vars.os == "all")) {
            Core.raise(ValueError(
                "The 'arch' and 'os' variables should not be all."));
        }

        //
        // Populate the outgoing dictionary with information about the package.
        //

        installScript = vars.get("script");
        info = {
            "name": vars.name,
            "version": vars.version,
            "release": vars.release,
            "section": vars.section,
            "description": vars.description,
            "url": vars.url,
            "arch": vars.arch,
            "os": vars.os,
            "license": vars.license,
            "depends": vars.depends,
            "conffiles": vars.conffiles,
        };

        (os.chdir)(this.datadir);
        contents = (os.listdir)(this.datadir);

        //
        // Create the data archive.
        //

        archive = Archive.open("data.cpio.lz", "w");
        for (content in contents) {
            archive.add(content, content);
        }

        archive.close();
        stats = archive.stats();
        info["size"] = stats.compressedSize;
        info["installed_size"] = stats.uncompressedSize;

        //
        // Write out the info.json file.
        //

        file = (io.open)("info.json", "w");
        file.write((json.dumps)(info, 4));
        file.close();

        //
        // Create the final CPIO archive. Always add the info.json first so
        // it's easy to find during package enumeration.
        //

        if (outputDirectory == null) {
            outputDirectory = ".";
        }

        finalPath = "%s/%s-%s.gft" % [outputDirectory, vars.name, vars.version];
        archive = (cpio.CpioArchive)(finalPath, "w");
        archive.add("info.json", "info.json", false, null);
        if (installScript) {
            archive.add(installScript, "install.ck", false, null);
        }

        archive.add("data.cpio.lz", "data.cpio.lz", false, null);
        archive.close();

        //
        // Remove the temporary files.
        //

        (os.unlink)("info.json");
        (os.unlink)("data.cpio.lz");
        return;
    }
}

class PackageIndexBuilder {
    var _packageList;
    var _archFilter;
    var _osFilter;
    var _failedPackages;
    var _errors;

    function
    __init (
        directory
        )

    /*++

    Routine Description:

        This routine instantiates a package index builder.

    Arguments:

        directory - Supplies the directory where the index should be created.

    Return Value:

        None.

    --*/

    {

        this.directory = directory;
        return this;
    }

    function
    build (
        output,
        archFilter,
        osFilter,
        errors
        )

    /*++

    Routine Description:

        This routine builds a package index.

    Arguments:

        output - Supplies an optional path where the index file should be
            created. If null, then the file will be created in the package
            directory.

        archFilter - Supplies an optional string or list of architectures to
            include in the index. Supply null to include all.

        osFilter - Supplies an optional string or list of OSes to include in
            the index. Supply null to include all.

        errors - Supplies an indicator of what to do when one or more packages
            fails to build. Valid values are null or "none" (ignore errors),
            "warn" (print and return errors), "fail" (print errors and raise
            an exception), or "failfast" (raise on the first exception that
            occurs).

    Return Value:

        Null on success.

        Returns a list where each element contains a list of the package path
        that failed, and the exception.

    --*/

    {

        var exception;
        var file;

        if (archFilter != null) {
            if (archFilter is String) {
                if (archFilter == "all") {
                    archFilter = null;

                } else {
                    archFilter = archFilter.split(null, -1);
                }
            }
        }

        if (osFilter != null) {
            if (osFilter is String) {
                if (osFilter == "all") {
                    osFilter = null;

                } else {
                    osFilter = osFilter.split(null, -1);
                }
            }
        }

        _archFilter = archFilter;
        _osFilter = osFilter;
        _packageList = [];
        _failedPackages = [];
        _errors = errors;
        this._enumeratePackages(this.directory);

        //
        // Compress and write out the JSON index.
        //

        if (output == null) {
            output = this.directory + "/index.json.lz";
        }

        file = (lzfile.LzFile)(output, "w", 9);
        file = BufferedIo(file, 0);
        file.write((json.dumps)(_packageList, 1));
        file.close();
        if (_failedPackages.length() == 0) {
            return null;
        }

        if ((errors == null) || (errors == "none")) {
            return _failedPackages;
        }

        for (failure in _failedPackages) {
            Core.print("Package %s could not be added to the index:" %
                       failure[0]);

            Core.print(failure[1]);
        }

        if (errors == "warn") {
            return _failedPackages;
        }

        if (errors != "fail") {
            Core.raise(ValueError("Invalid errors value '%s'" % errors));
        }

        exception = PackageIndexError(
            "%d package%s could not be added to the index" %
            [_failedPackages.length(),
             _failedPackages.length() == 1 ? "" : "s"]);

        exception.failures = _failedPackages;
        Core.raise(exception);
    }

    function
    _enumeratePackages (
        path
        )

    /*++

    Routine Description:

        This routine enumerates packages within the given path inside the
        given path.

    Arguments:

        path - Supplies the path to enumerate.

    Return Value:

        None. The enumreated packages are added to the internal package list
        field.

    --*/

    {

        var contents = (os.listdir)(path);
        var dirs = [];
        var finalPath;

        for (element in contents) {
            finalPath = path + "/" + element;
            if ((os.isdir)(finalPath)) {
                dirs.append(finalPath);

            } else if (finalPath.endsWith(".gft")) {
                this._enumeratePackage(finalPath);
            }
        }

        for (dir in dirs) {
            this._enumeratePackages(dir);
        }

        return;
    }

    function
    _enumeratePackage (
        path
        )

    /*++

    Routine Description:

        This routine enumerates a single package.

    Arguments:

        path - Supplies the path of the package.

    Return Value:

        None. The enumreated packages are added to the internal package list
        field.

    --*/

    {

        var archive;
        var indexInfo;
        var info;
        var member;
        var memberFile;

        //
        // Attempt to get the package info, but defer failures until later.
        //

        try {
            archive = (cpio.CpioArchive)(path, "r");
            member = archive.getMember("info.json");
            memberFile = archive.extractFile(member);
            info = (json.loads)(memberFile.readall());

            //
            // If the package passes the architecture and OS filters, add it to
            // the list.
            //

            if ((_osFilter == null) || (_osFilter.contains(info.os))) {
                if ((_archFilter == null) ||
                    (_archFilter.contains(info.arch))) {

                    indexInfo = {
                        "name": info.name,
                        "version": info.version,
                        "release": info.release,
                        "section": info.section,
                        "depends": info.depends,
                        "arch": info.arch,
                        "os": info.os,
                        "file": path[(this.directory.length() + 1)...-1]
                    };

                    _packageList.append(indexInfo);
                }
            }

            memberFile.close();
            archive.close();

        } except Exception as e {
            if (_errors == "failfast") {
                Core.raise(e);
            }

            _failedPackages.append([path, e]);
        }

        return;
    }

}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    patchman.ck

Abstract:

    This module implements the patch manager object within Santa.

Author:

    Evan Green 25-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import io;
import os;
from santa.config import config;
from santa.file import cptree, link, mkdir, path, rmtree;
from santa.lib.config import ConfigFile;
from santa.lib.diff import DiffSet;
import santa.lib.patch;
from santa.lib.santaconfig import SANTA_PATCH_CONFIG_PATH;

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

class PatchManager {
    var _config;
    var _patches;

    static
    function
    load (
        )

    /*++

    Routine Description:

        This routine loads the patch manager configuration from the current
        user and initializes a new patch manager instance.

    Arguments:

        None.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        var instance = PatchManager();

        instance._loadConfig();
        return instance;
    }

    function
    __init (
        srcdir,
        patchdir
        )

    /*++

    Routine Description:

        This routine initializes a new patch manager.

    Arguments:

        srcdir - Supplies the source directory to manage.

        patchdir - Supplies the patch directory.

    Return Value:

        Returns an initialized class instance.

    --*/

    {

        srcdir = srcdir.replace("\\", "/", -1);
        patchdir = patchdir.replace("\\", "/", -1);
        while (srcdir[-1] == "/") {
            srcdir = srcdir[0..-1];
        }

        while (patchdir[-1] == "/") {
            patchdir = patchdir[0..-1];
        }

        this.srcdir = srcdir;
        this.patchdir = patchdir;
        this.files = {};
        mkdir(this.patchdir);
        _patches = null;
        return this;
    }

    function
    add (
        filePath
        )

    /*++

    Routine Description:

        This routine adds a file to the list of files to compare.

    Arguments:

        filePath - Supplies the path of the file to add.

    Return Value:

        None. An exception is raised on failure.

    --*/

    {

        var destination = this.patchdir + "/current/";
        var elements;
        var source = this._findSource(filePath);
        var sourcePath = this.srcdir + "/" + source;

        if (source == "") {
            sourcePath = this.srcdir;
        }

        //
        // Copy the file into a temporary location so it can be diffed later.
        //

        destination += source;
        if ((os.exists)(sourcePath)) {
            if ((os.isdir)(sourcePath)) {
                elements = (os.listdir)(sourcePath);
                for (element in elements) {
                    this.add(sourcePath + "/" + element);
                }

            } else {
                mkdir((os.dirname)(destination));
                cptree(sourcePath, destination);
                this.files[source] = true;
            }

        } else {
            this.files[source] = true;
        }

        return;
    }

    function
    remove (
        filePath
        )

    /*++

    Routine Description:

        This routine removes a file from the list of files to compare.

    Arguments:

        filePath - Supplies the path of the file to remove.

    Return Value:

        None. An exception is raised on failure.

    --*/

    {

        var elements;
        var source = this._findSource(filePath);
        var sourcePath = this.srcdir + "/" + source;

        if (source == "") {
            sourcePath = this.srcdir;
        }

        if ((os.exists)(sourcePath)) {
            if ((os.isdir)(sourcePath)) {
                elements = (os.listdir)(sourcePath);
                for (element in elements) {
                    this.remove(sourcePath + "/" + element);
                }

            } else {
                this.files.remove(source);
            }

        } else {
            this.files.remove(source);
        }

        return;
    }

    function
    save (
        )

    /*++

    Routine Description:

        This routine saves the current patch manager state as the global user
        state.

    Arguments:

        None.

    Return Value:

        None. An exception is raised on failure.

    --*/

    {

        this._loadConfig();
        this._loadPatches();

        _config.setKey("srcdir", this.srcdir);
        _config.setKey("patchdir", this.patchdir);
        _config.setKey("files", this.files.keys());
        _config.setKey("patches", _patches);
        _config.save();
        return;
    }

    function
    applyPatch (
        number
        )

    /*++

    Routine Description:

        This routine applies the given patch number, and marks it as applied.
        If the patch is already applied, it does nothing.

    Arguments:

        number - Supplies the patch number to apply. Supply a negative number
            to reverse the patch and mark it as unapplied.

    Return Value:

        None. An exception is raised on failure.

    --*/

    {

        var apply = true;
        var element;
        var options = 0;
        var patchset;

        if (number == 0) {
            Core.raise(ValueError("Zero is not a valid patch number"));
        }

        if (number < 0) {
            apply = false;
            number = -number;
            options |= patch.PATCH_OPTION_REVERSE;
        }

        this._loadConfig();
        this._loadPatches();
        element = _patches.get(number);
        if (element == null) {
            Core.raise(ValueError("No such patch number: %d" % number));
        }

        if (element.applied == apply) {
            return;
        }

        //
        // Load up the patch.
        //

        patchset = (patch.PatchSet)(this.patchdir + "/" + element.name);
        patchset.apply(this.srcdir, options, 1);
        element.applied = apply;
        return;
    }

    function
    applyTo (
        number
        )

    /*++

    Routine Description:

        This routine applies up to and including the given patch number, or
        unapplies patches down to but not including the given patch number.

    Arguments:

        number - Supplies the patch number to apply. Supply zero to unapply
            all patches. Supply -1 to apply all patches.

    Return Value:

        None. An exception is raised on failure. If some patches are
        successfully applied/unapplied, they will be marked as such.

    --*/

    {

        var max = 0;

        this._loadConfig();
        this._loadPatches();
        for (patch in _patches) {
            if (patch > max) {
                max = patch;
            }
        }

        if (number < 0) {
            number = max;
        }

        //
        // Un-apply everything above the number.
        //

        if (max > number) {
            for (index in max..number) {
                if (_patches.get(index)) {
                    this.applyPatch(-index);
                }
            }
        }

        //
        // Apply everything below or equal to the number.
        //

        if (number > 0) {
            for (index in 1...number) {
                if (_patches.get(index)) {
                    this.applyPatch(index);
                }
            }
        }

        return;
    }

    function
    markPatches (
        patches,
        applied
        )

    /*++

    Routine Description:

        This routine simply marks a single patch number or a range of patches
        as applied or unapplied, without actually doing any editing. This is
        useful for manual situations where things have gotten out of sync.

    Arguments:

        patches - Supplies either an integer of the patch number to mark, or
            a range of patches to mark. Supply -1 to mark all patches.

        applied - Supplies a boolean indicating whether to mark the patches as
            applied (true) or not applied (false).

    Return Value:

        None.

    --*/

    {

        this._loadConfig();
        this._loadPatches();
        if (patches is Int) {
            if (patches == -1) {
                for (patch in _patches) {
                    _patches[patch].applied = applied;
                }

            } else {
                _patches[patches].applied = applied;
            }

        } else if (patches is Range) {
            for (patch in patches) {
                if (_patches.get(patch)) {
                    _patches[patch].applied = applied;
                }
            }

        } else {
            Core.raise(TypeError("Expected integer or range"));
        }

        return;
    }

    function
    commit (
        name,
        message,
        force
        )

    /*++

    Routine Description:

        This routine commits the diffs for the set of files marked for patching
        in the current commit.

    Arguments:

        name - Supplies the name portion of the patch file. Spaces will be
            replaced with dashes.

        message - Supplies an optional message to write at the front of the
            patch file.

        force - Supplies a boolean indicating if the patch should be replaced
            if one already exists for the given number.

    Return Value:

        None.

    --*/

    {

        var diff;
        var diffset = this.currentDiffSet();
        var file;
        var finalName = name.replace(" ", "-", -1);
        var finalPath;
        var maxApplied = 0;
        var patch;
        var removePath;
        var number;

        //
        // Create the diff first.
        //

        if (this.files.length() == 0) {
            Core.raise(ValueError("Empty diff"));
        }

        diff = diffset.unifiedDiff();

        //
        // Figure out what number this patch should have.
        //

        number = 1;
        while (true) {
            patch = _patches.get(number);
            if (patch == null) {
                break;
            }

            if (patch.applied == false) {
                if (force == false) {
                    Core.raise(ValueError("Patch %d already exists: %s. "
                                          "Either apply it or remove it" %
                                          [number, _patches[number].name]));
                }

                break;
            }

            number += 1;
        }

        //
        // Create the patch.
        //

        finalName = "%03d-%s.patch" % [number, finalName];
        finalPath = this.patchdir + "/" + finalName;
        file = (io.open)(finalPath, "w");
        if (message) {
            file.write(message);
            if (!message.endsWith("\n")) {
                file.write("\n");
            }
        }

        file.write(diff);
        file.close();
        _patches[number] = {
            "name": finalName,
            "applied": true
        };

        this.files = {};
        Core.print("Saved patch %s" % finalName);

        //
        // Remove the saved original files.
        //

        rmtree(this.patchdir + "/current");

        //
        // Remove the original patch if forced to.
        //

        if (patch != null) {
            if (patch.name != finalName) {
                finalPath = this.patchdir + "/" + patch.name;
                (os.unlink)(finalPath);
                Core.print("Removed patch %s" % finalPath);
            }
        }

        return 0;
    }

    function
    currentDiffSet (
        )

    /*++

    Routine Description:

        This routine returns a diff set for the current patch.

    Arguments:

        None.

    Return Value:

        Returns a DiffSet initialized with the current set of changes.

    --*/

    {

        var diff;
        var diffset = DiffSet();
        var length;
        var oldcwd = (os.getcwd)();
        var subOut;

        this._loadConfig();
        this._loadPatches();
        (os.chdir)(this.srcdir);
        for (element in this.files) {
            diffset.add(this.patchdir + "/current/" + element,
                        "./" + element);
        }

        (os.chdir)(oldcwd);
        subOut = this.patchdir + "/current";
        length = subOut.length();
        for (element in diffset) {
            if (element.left.name.startsWith(subOut)) {
                element.left.name = "a" + element.left.name[length...-1];
            }

            if (element.right.name.startsWith("./")) {
                element.right.name = "b" + element.right.name[1...-1];
            }
        }

        return diffset;
    }

    function
    deletePatch (
        number,
        shift
        )

    /*++

    Routine Description:

        This routine removes a patch from the patch directory.

    Arguments:

        number - Supplies the number to remove.

        shift - Supplies a boolean indicating whether or not to shift all
            remaining patches down.

    Return Value:

        None.

    --*/

    {

        var data;
        var file;
        var max = 0;
        var oldname;
        var newname;
        var patch;

        this._loadConfig();
        this._loadPatches();
        patch = _patches[number];
        rmtree(this.patchdir + "/" + patch.name);
        _patches.remove(number);
        if (!shift) {
            return;
        }

        for (element in _patches) {
            if (element > max) {
                max = element;
            }
        }

        if (max <= number) {
            return;
        }

        for (element in number..max) {
            patch = _patches.get(element + 1);
            if (!patch) {
                continue;
            }

            _patches[element] = _patches[element + 1];
            _patches.remove(element + 1);
            oldname = this.patchdir + "/" + patch.name;

            //
            // Read in the old file contents.
            //

            file = (io.open)(oldname, "r");
            data = file.readall();
            file.close();

            //
            // Write that out to the new file.
            //

            newname = "%03d-%s" % [element, patch.name.split("-", 1)[1]];
            file = (io.open)(this.patchdir + "/" + newname, "w");
            file.write(data);
            file.close();

            //
            // Remove the old file.
            //

            (os.unlink)(oldname);
            patch.name = newname;
        }

        return;
    }

    function
    edit (
        number
        )

    /*++

    Routine Description:

        This routine returns to just before the specified patch, adds the
        files from the patch, then applies the patch.

    Arguments:

        number - Supplies the number of the patch to edit.

    Return Value:

        None.

    --*/

    {

        var element;
        var filePath;
        var patchset;
        var pathSplit = 1;

        this._loadConfig();
        this._loadPatches();
        if (number < 1) {
            Core.raise(ValueError("Supply a patch number starting at 1"));
        }

        if (this.files.length() != 0) {
            Core.raise(ValueError("Current patchset is not empty"));
        }

        this.applyTo(number - 1);
        element = _patches.get(number);
        if (element.applied) {
            Core.raise(ValueError("Patch should not be applied"));
        }

        patchset = (patch.PatchSet)(this.patchdir + "/" + element.name);

        //
        // Add all the files in the patch to the current changeset before they
        // get molested.
        //

        for (file in patchset) {
            filePath = this.srcdir + "/" +
                       file.destinationFile.split("/", pathSplit)[-1];

            this.add(filePath);
        }

        //
        // Apply the patch, but don't mark it as applied.
        //

        patchset.apply(this.srcdir, 0, pathSplit);
        return 0;
    }


    function
    _loadConfig (
        )

    /*++

    Routine Description:

        This routine loads up the user's patch manager configuration.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var files;
        var patchdir;

        if (_config) {
            return;
        }

        _config = ConfigFile(path(SANTA_PATCH_CONFIG_PATH), {"files": []});

        //
        // Use this instance's variables if they've been populated.
        //

        try {
            this.srcdir;
            patchdir = this.patchdir;

            //
            // Clear the patches if the patch directory has changed.
            //

            if (patchdir != _config.getKey("patchdir")) {
                _patches = {};
                this.files = {};
            }

        //
        // Load from the config file if the patchdir has not yet been assigned.
        //

        } except KeyError {
            this.srcdir = _config.getKey("srcdir");
            this.patchdir = _config.getKey("patchdir");
            _patches = _config.getKey("patches");
            files = {};
            for (file in _config.getKey("files")) {
                files[file] = true;
            }

            this.files = files;
        }

        return;
    }

    function
    _loadPatches (
        )

    /*++

    Routine Description:

        This routine loads up the available patches.

    Arguments:

        None.

    Return Value:

        None.

    --*/

    {

        var directory = (os.listdir)(this.patchdir);
        var element;
        var number;

        if (_patches == null) {
            _patches = {};
        }

        for (file in directory) {
            try {
                number = Int.fromString(file.split("-", 1)[0]);

            } except ValueError {
                continue;
            }

            //
            // If there's no concept of that patch, create a new element and
            // mark it as unapplied.
            //

            element = _patches.get(number);
            if (element == null) {
                element = {
                    "name": file,
                    "applied": true
                };

                _patches[number] = element;
            }
        }

        return;
    }

    function
    _findSource (
        filePath
        )

    /*++

    Routine Description:

        This routine finds the absolute path to a file relative to the source
        root.

    Arguments:

        filePath - Supplies the file path.

    Return Value:

        Returns the path to the source relative to the source directory.

    --*/

    {

        var baseName;
        var dirName;
        var finalPath = path(filePath);
        var originalCwd = (os.getcwd)();
        var srcdir = this.srcdir;

        filePath = finalPath;
        if (!(this.srcdir)) {
            Core.raise(ValueError("srcdir parameter not set"));
        }

        //
        // If the path is not absolute, create an absolute path.
        //

        if (((filePath.length() > 1) && (filePath[1] != ":")) &&
            (filePath[0] != "/")) {

            baseName = (os.basename)(filePath);
            if (baseName == ".") {
                baseName = "";
            }

            dirName = (os.dirname)(filePath);

            //
            // Change to the directory where the file lives to get the real
            // path.
            //

            (os.chdir)(dirName);
            finalPath = (os.getcwd)() + "/" + baseName;
            (os.chdir)(originalCwd);
        }

        if (finalPath == srcdir) {
            return "";
        }

        srcdir += "/";
        if (!finalPath.startsWith(srcdir)) {
            Core.raise(ValueError("Path %s does not appear to be inside of "
                                  "source directory %s" %
                                  [finalPath, srcdir]));
        }

        finalPath = finalPath[srcdir.length()...-1];
        while ((finalPath != "") && (finalPath[0] == "/")) {
            finalPath = finalPath[1...-1];
        }

        return finalPath;
    }
}

//
// --------------------------------------------------------- Internal Functions
//


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    file.ck

Abstract:

    This module contains file utilities for the Santa application.

Author:

    Evan Green 24-May-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import io;
import os;

from santa.config import config;

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

var CP_CHUNK_SIZE = 1048576;

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

function
path (
    filepath
    )

/*++

Routine Description:

    This routine returns the adjusted path for the given file. The root will
    be added on, and if the given path starts with a ~ it will be converted to
    the current home directory.

Arguments:

    filepath - Supplies the file path to convert.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    var root = config.getKey("core.root");

    if (filepath[0...1] == "~/") {
        filepath = config.getKey("core.home") + filepath[1...-1];
    }

    if (!root || (root.length() == 0)) {
        return filepath;
    }

    if (root[-1] != "/") {
        root = root + "/";
    }

    return root + filepath;
}

function
open (
    filepath,
    mode
    )

/*++

Routine Description:

    This routine is the same as the open function in the standard io module
    except that it munges the path to prepend the root and trade tildes for
    the home directory.

Arguments:

    path - Supplies the path to open.

    mode - Supplies the mode to open with.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    if (filepath is String) {
        filepath = path(filepath);
    }

    return (io.open)(filepath, mode);
}

function
mkdir (
    filepath
    )

/*++

Routine Description:

    This routine creates a directory (munging the path if there is a root or ~
    in the path), creating intermediate directories along the way.

Arguments:

    filepath - Supplies the directory path to create.

Return Value:

    0 on success.

    Raises an exception if directory creation failed.

--*/

{

    var components;
    var osMkdir = os.mkdir;
    var totalpath;

    filepath = path(filepath);
    try {
        osMkdir(filepath, 0775);

    } except os.OsError as e {
        if (e.errno == os.EEXIST) {
            return 0;
        }
    }

    filepath = filepath.replace("\\", "/", -1);
    components = filepath.split("/", -1);
    totalpath = null;
    for (component in components) {
        if (totalpath == null) {
            totalpath = component;
            continue;
        }

        totalpath = "/".join([totalpath, component]);
        try {
            osMkdir(totalpath, 0775);

        } except os.OsError as e {
            if (e.errno != os.EEXIST) {
                Core.raise(e);
            }
        }
    }

    return 0;
}

function
rmdir (
    filepath
    )

/*++

Routine Description:

    This routine removes a directory (munging the path if there is a root or ~
    in the path). The directory must be empty

Arguments:

    filepath - Supplies the directory path to remove.

Return Value:

    0 on success.

    Raises an exception if directory removal failed.

--*/

{

    return (os.rmdir)(path(filepath));
}

function
unlink (
    filepath
    )

/*++

Routine Description:

    This routine is the same as the unlink function in the standard io module
    except that it munges the path to prepend the root and trade tildes for
    the home directory.

Arguments:

    path - Supplies the path to unlink.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    filepath = path(filepath);
    return (os.unlink)(filepath);
}

function
_rmtree (
    filepath
    )

/*++

Routine Description:

    This routine removes a given path and any subdirectories it may have: the
    equivalent of rm -rf. It does not translate the path based on the root.

Arguments:

    filepath - Supplies the directory path to remove.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    var contents;
    var verbose = config.getKey("core.verbose");

    if (!(os.lexists)(filepath)) {
        return;
    }

    if (((os.islink)(filepath)) || (!(os.isdir)(filepath))) {
        if (verbose) {
            Core.print("Removing %s" % filepath);
        }

        try {
            return (os.unlink)(filepath);

        } except os.OsError as e {

            //
            // If the caller doesn't have permission to delete the file, try to
            // gain permission. Windows for some reason returns EINVAL
            // sometimes.
            //

            if ((e.errno == os.EACCES) || (e.errno == os.EINVAL)) {
                try {
                    (os.chmod)(filepath, 0666);

                //
                // On failure to change permissions, raise the original error.
                //

                } except os.OsError {
                    Core.raise(e);
                }

                return (os.unlink)(filepath);

            } else {
                Core.raise(e);
            }
        }
    }

    contents = (os.listdir)(filepath);
    for (element in contents) {
        if ((!(os.islink)(filepath)) && ((os.isdir)(filepath))) {
            _rmtree("/".join([filepath, element]));
        }
    }

    if (verbose) {
        Core.print("Removing %s" % filepath);
    }

    try {
        return (os.rmdir)(filepath);

    } except os.OsError as e {

        //
        // Again, if the caller doesn't have permission, try to gain permission.
        //

        if (e.errno == os.EACCES) {
            try {
                (os.chmod)(filepath, 0666);

            } except os.OsError {
                Core.raise(e);
            }

            return (os.rmdir)(filepath);
        }

        Core.raise(e);
    }

    return;
}

function
rmtree (
    filepath
    )

/*++

Routine Description:

    This routine removes a given path and any subdirectories it may have: the
    equivalent of rm -rf.

Arguments:

    filepath - Supplies the directory path to remove.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    return _rmtree(path(filepath));
}

function
_cptree (
    source,
    destination
    )

/*++

Routine Description:

    This routine recursively copies a path. It does not translate the path
    based on the root.

Arguments:

    filepath - Supplies the directory path to remove.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    var chunk;
    var contents;
    var infile;
    var link;
    var outfile;
    var stat;
    var verbose;

    //
    // Recurse if it's a directory.
    //

    stat = (os.lstat)(source);
    if ((stat.st_mode & os.S_IFMT) == os.S_IFDIR) {
        contents = (os.listdir)(source);
        if (!(os.isdir)(destination)) {
            (os.mkdir)(destination, 0775);

        } else {
            destination += "/" + (os.basename)(source);
            if (!(os.isdir)(destination)) {
                (os.mkdir)(destination, 0775);
            }
        }

        for (element in contents) {
            _cptree("/".join([source, element]),
                    "/".join([destination, element]));
        }

    //
    // Create a symbolic link.
    //

    } else if ((stat.st_mode & os.S_IFMT) == os.S_IFLNK) {
        verbose = config.getKey("core.verbose");
        link = (os.readlink)(source);
        if (verbose) {
            Core.print("Copying %s [%s] -> %s" %
                       [source, link, destination]);
        }

        try {
            (os.symlink)(link, destination);

        } except os.OsError as e {
            if (e.errno == os.EEXIST) {
                (os.unlink)(destination);
                (os.symlink)(link, destination);
            }
        }

    //
    // Copy a regular file.
    //

    } else if ((stat.st_mode & os.S_IFMT) == os.S_IFREG) {

        //
        // Skip null copies.
        //

        if (source == destination) {
            return;
        }

        //
        // If the destination is a directory, copy the source file inside
        // the directory.
        //

        if ((os.isdir)(destination)) {
            destination += "/" + (os.basename)(source);
        }

        verbose = config.getKey("core.verbose");
        if (verbose) {
            Core.print("Copying %s -> %s" % [source, destination]);
        }

        infile = (io.open)(source, "rb");
        outfile = (io.open)(destination, "wb");
        while (true) {
            chunk = infile.read(CP_CHUNK_SIZE);
            if (chunk.length() == 0) {
                break;
            }

            outfile.write(chunk);
        }

        infile.close();
        outfile.close();

    } else {
        if (config.getKey("core.verbose")) {
            Core.print("Warning: Skipping irregular file: %s" % source);
        }
    }

    if ((stat.st_mode & os.S_IFMT) != os.S_IFLNK) {
        (os.chmod)(destination, stat.st_mode & 03777);
    }

    return;
}

function
cptree (
    source,
    destination
    )

/*++

Routine Description:

    This routine recursively copies a directory tree, translating both the
    source and the destination paths.

Arguments:

    source - Supplies the source path to copy.

    destination - Supplies the destination of the copy.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    return _cptree(path(source), path(destination));
}

function
chdir (
    filepath
    )

/*++

Routine Description:

    This routine changes to the given directory, translating the path first.

Arguments:

    filepath - Supplies the directory path to the new working directory.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    filepath = path(filepath);
    if (config.getKey("core.verbose")) {
        Core.print("Changing directory to %s" % filepath);
    }

    return (os.chdir)(filepath);
}

function
mv (
    source,
    destination
    )

/*++

Routine Description:

    This routine moves a file or directory. It attempts to do a rename, and
    falls back on cptree + rmtree.

Arguments:

    source - Supplies the source path to copy.

    destination - Supplies the destination of the copy.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    try {
        (os.rename)(path(source), path(destination));
        if (config.getKey("core.verbose")) {
            Core.print("Moving %s -> %s" % [path(source), path(destination)]);
        }

    } except os.OsError as e {
        if (e.errno != os.EXDEV) {
            Core.raise(e);
        }
    }

    cptree(source, destination);
    if (source != destination) {
        rmtree(source);
    }

    return;
}

function
chroot (
    filepath
    )

/*++

Routine Description:

    This routine changes the root to the given directory, translating the path
    first.

Arguments:

    filepath - Supplies the directory path to the new root directory.

Return Value:

    0 on success.

    Raises an exception if the change failed.

--*/

{

    filepath = path(filepath);
    if (config.getKey("core.verbose")) {
        Core.print("Changing root to %s" % filepath);
    }

    return (os.chroot)(filepath);
}

function
chmod (
    filepath,
    mode
    )

/*++

Routine Description:

    This routine changes the permissions on the given path.

Arguments:

    filepath - Supplies the path to set permissions for.

    mode - Supplies the new permissions to set.

Return Value:

    0 on success.

    Raises an exception if the operation failed.

--*/

{

    filepath = path(filepath);
    if (config.getKey("core.verbose")) {
        Core.print("chmod %o %s" % [mode, filepath]);
    }

    return (os.chmod)(filepath, mode);
}

function
link (
    existingpath,
    newpath
    )

/*++

Routine Description:

    This routine creates a new hard link, translating the path first.

Arguments:

    existingpath - Supplies the existing path to create the link from.

    newpath - Supplies the path where the new hard link should be created.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    existingpath = path(existingpath);
    newpath = path(newpath);
    if (config.getKey("core.verbose")) {
        Core.print("Linking %s to %s" % [existingpath, newpath]);
    }

    return (os.link)(existingpath, newpath);
}

function
symlink (
    linktarget,
    linkname
    )

/*++

Routine Description:

    This routine creates a new symbolic link, translating the path first.

Arguments:

    linktarget - Supplies the target where the link should point to. This path
        will not be adjusted by the root.

    linkname - Supplies the path where the new symbolic link should go. This
        will be adjusted by the root.

Return Value:

    0 on success.

    Raises an exception if removal failed.

--*/

{

    linkname = path(linkname);
    if (config.getKey("core.verbose")) {
        Core.print("Symlinking %s to %s" % [linktarget, linkname]);
    }

    return (os.symlink)(linktarget, linkname);
}

function
exists (
    name
    )

/*++

Routine Description:

    This routine determines if the given path exists. If the target is a
    symbolic link, it is followed to determine if the link target exists.

Arguments:

    name - Supplies the path to check.

Return Value:

    true if the path exists.

    false if the path does not exist.

--*/

{

    return (os.exists)(path(name));
}

function
lexists (
    name
    )

/*++

Routine Description:

    This routine determines if the given path or symbolic link exists.

Arguments:

    name - Supplies the path to check.

Return Value:

    true if the path exists.

    false if the path does not exist.

--*/

{

    return (os.lexists)(path(name));
}

function
isdir (
    name
    )

/*++

Routine Description:

    This routine determines if the given path is a directory.

Arguments:

    name - Supplies the path to check.

Return Value:

    true if the path exists and is a directory.

    false if the path does not exist or is not a directory.

--*/

{

    return (os.isdir)(path(name));
}

function
createStandardPaths (
    )

/*++

Routine Description:

    This routine is the same as the open function in the standard io module
    except that it munges the path to prepend the root and trade tildes for
    the home directory.

Arguments:

    path - Supplies the path to open.

    mode - Supplies the mode to open with.

Return Value:

    0 on success.

    1 on failure.

--*/

{

    var rootContainer = config.getKey("realm.root.containment.path");

    if (rootContainer && (rootContainer != "/")) {
        mkdir(rootContainer);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//


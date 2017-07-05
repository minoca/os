/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    defaultbuild.ck

Abstract:

    This module contains default functions for building packages.

Author:

    Evan Green 30-Jun-2017

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

import os;
from santa.build import shell;
from santa.config import config;
from santa.file import chdir, mkdir, rmtree, cptree;
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

function
defaultInit (
    build
    )

/*++

Routine Description:

    This routine implements the default init function, which throws up its
    hands in despair.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    Core.raise(ValueError("'%s' is missing an init function" % build.filePath));
}

function
defaultFetch (
    build
    )

/*++

Routine Description:

    This routine implements the default fetch function, which grabs any needed
    files and puts them in the source directory.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    var source;
    var vars = build.vars;

    try {
        source = vars.source;

    } except KeyError {
        vars.source = [];
        return;
    }

    if (!(source is List)) {
        source = source.split(null, -1);
        vars.source = source;
    }

    chdir(vars.startdir);

    //
    // Loop over each source in the source list.
    //

    for (element in source) {

        //
        // This would be the place to check the file location and potentially
        // download the file. For now, just stat each file, since only local
        // files are supported.
        //

        (os.stat)(element);
    }

    return;
}

function
defaultUnpack (
    build
    )

/*++

Routine Description:

    This routine implements the default unpack function, which extracts any
    archives.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    var archive;
    var sources;
    var vars = build.vars;
    var verbose = vars.verbose;

    chdir(vars.startdir);
    sources = vars.source;
    for (source in sources) {
        if (!source.endsWith(".cpio.lz")) {
            if (verbose) {
                Core.print("Copying %s to %s" % [source, vars.srcdir]);
            }

            cptree(source, vars.srcdir);
            continue;
        }

        if (verbose) {
            Core.print("Extracting %s to %s" % [source, vars.srcdir]);
        }

        archive = Archive.open(source, "r");
        archive.extractAll(vars.srcdir, -1, -1, true);
    }

    return;
}

function
defaultPrepare (
    build
    )

/*++

Routine Description:

    This routine implements the default prepare function, which applies any
    patches found.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    //
    // TODO: Apply any patches found.
    //

    return;
}

function
defaultConfigure (
    build
    )

/*++

Routine Description:

    This routine implements the default configure function, which does nothing.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    return;
}

function
defaultBuild (
    build
    )

/*++

Routine Description:

    This routine implements the default build function, which runs make and
    make install DESTDIR=$pkgdir.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    var vars = build.vars;

    chdir(vars.builddir);
    shell("make");
    shell("make install DESTDIR=" + vars.pkgdir);
    return;
}

function
defaultCheck (
    build
    )

/*++

Routine Description:

    This routine implements the default check function, which does nothing.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    return;
}

function
defaultPackage (
    build
    )

/*++

Routine Description:

    This routine implements the default package function, which creates a
    package from the pkgdir contents.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    return;
}

function
defaultClean (
    build
    )

/*++

Routine Description:

    This routine implements the default clean function, which removes the
    srcdir, builddir, and pkgdir.

Arguments:

    build - Supplies the build manager object.

Return Value:

    None.

--*/

{

    var vars = build.vars;
    var verbose = vars.verbose;

    if (verbose) {
        Core.print("Removing %s" % vars.pkgdir);
    }

    rmtree(vars.pkgdir);
    if (verbose) {
        Core.print("Removing %s" % vars.builddir);
    }

    rmtree(vars.builddir);
    if (verbose) {
        Core.print("Removing %s" % vars.srcdir);
    }

    rmtree(vars.srcdir);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//


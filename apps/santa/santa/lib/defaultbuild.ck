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
from santa.build import shell, make;
from santa.config import config;
from santa.file import chdir, mkdir, path, rmtree, cptree;
from santa.lib.archive import Archive;
from santa.lib.patchman import PatchManager;

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

        try {
            (os.stat)(element);

        } except os.OsError {
            Core.raise(ValueError("Failed to stat source: %s" % element));
        }
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
    if (!(sources is List)) {
        sources = sources.split(null, -1);
        vars.source = sources;
    }

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

    var patchdir;
    var patchman;
    var vars = build.vars;
    var verbose = vars.verbose;

    patchdir = vars.get("patchdir");
    if (patchdir == null) {
        patchdir = vars.srcdir + "/patches";
    }

    //
    // Create a new temporary patch manager for the given source and patch
    // directory. Then apply all patches.
    //

    if ((os.isdir)(patchdir)) {
        patchman = PatchManager(vars.srcdir, patchdir);
        patchman.markPatches(-1, false);
        patchman.applyTo(-1);
    }

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

    var pkgdir;
    var vars = build.vars;

    //
    // On Windows, remove the drive letter since too many things get fouled up
    // by not having / as the first character.
    //

    pkgdir = vars.pkgdir;
    if (os.system == "Windows") {
        if ((pkgdir.length() > 2) && (pkgdir[1] == ":")) {
            pkgdir = pkgdir[2...-1];
        }
    }

    chdir(vars.builddir);
    make(vars.flags.get("parallel") != false, "");
    make(vars.flags.get("parallel") != false, "install DESTDIR=%s" % pkgdir);
    shell("rm -f $pkgdir/usr/lib/*.la");
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
defaultPackagedoc (
    build
    )

/*++

Routine Description:

    This routine implements the default docs subpackage function, which
    moves anything in a doc-like directory off in /usr/share into the package.

Arguments:

    build - Supplies the build manager object.

Return Value:

    Returns true if the package has contents and should be created.

    Returns false if it turns out there's no need to create the package.

--*/

{

    var description;
    var vars = build.vars;

    description = vars.description;
    if (description.endsWith(".")) {
        description = description[0..-1];
    }

    vars.description = description + " (documentation)";
    vars.section = "doc";
    vars.depends = "";
    chdir(build.vars.pkgdir);
    shell("for d in doc man info html sgml licenses gtk-doc ri help; do\n"
          "if [ -d \"$pkgdir/usr/share/$d\" ]; then\n"
          "    mkdir -p \"$subpkgdir/usr/share\"\n"
          "    mv \"$pkgdir/usr/share/$d\" \"$subpkgdir/usr/share\"\n"
          "fi\n"
          "done\n"
          "rm -f \"$subpkgdir/usr/share/info/dir\"\n"
          "rmdir \"$pkgdir/usr/share\" \"$pkgdir/usr\" 2>/dev/null || :\n");

    //
    // If any files were added, create the package.
    //

    if ((os.listdir)(build.vars.subpkgdir).length() != 0) {
        return true;
    }

    return false;
}

function
defaultPackagelibs (
    build
    )

/*++

Routine Description:

    This routine implements the default libs subpackage function, which pulls
    out any .so or .dll files into the subpackage.

Arguments:

    build - Supplies the build manager object.

Return Value:

    Returns true if the package has contents and should be created.

    Returns false if it turns out there's no need to create the package.

--*/

{

    var description;
    var pattern = "\"$pkgdir\"/$d/lib*.so.[0-9]*";
    var vars = build.vars;

    if (build.vars.os == "Windows") {
        pattern = "\"$pkgdir\"/$d/*.dll";
    }

    description = vars.description;
    if (description.endsWith(".")) {
        description = description[0..-1];
    }

    vars.description = description + " (libraries)";
    vars.section = "lib";
    chdir(build.vars.pkgdir);
    shell("for d in lib usr/lib; do\n"
          "  for file in %s; do\n"
          "    [ -f $file ] || continue\n"
          "    mkdir -p \"$subpkgdir/$dir\"\n"
          "    mv \"$file\" \"$subpkgdir/$dir\"\n"
          "  done\n"
          "done\n" % pattern);

    //
    // If any files were added, create the package.
    //

    if ((os.listdir)(build.vars.subpkgdir).length() != 0) {
        return true;
    }

    return false;
}

function
defaultPackagedev (
    build
    )

/*++

Routine Description:

    This routine implements the default dev subpackage function, which pulls
    out includes, common build files, and files ending in .c, .h, .a, or .o.

Arguments:

    build - Supplies the build manager object.

Return Value:

    Returns true if the package has contents and should be created.

    Returns false if it turns out there's no need to create the package.

--*/

{

    var depends = [];
    var description;
    var name;
    var vars = build.vars;

    description = vars.description;
    if (description.endsWith(".")) {
        description = description[0..-1];
    }

    vars.description = description + " (development)";
    vars.section = "dev";

    //
    // By default the dev package depends on everything that's not dev and docs.
    //

    name = vars.name[0..-4];
    for (element in vars.depends) {
        if ((element != name + "-dev") &&
            (element != name + "-doc")) {

            depends.append(element);
        }
    }

    depends.append(name);
    vars.depends = depends;
    chdir(build.vars.pkgdir);
    shell("libdirs=usr\n"
          "[ -d lib ] && libdirs=\"$libdirs lib\"\n"
          "for i in usr/lib/pkgconfig usr/share/aclocal "
          "usr/share/gettext usr/bin/*-config "
          "usr/share/vala/vapi usr/share/gir-[0-9]* usr/share/qt*/mkspecs "
          "usr/lib/qt*/mkspecs usr/lib/cmake "
          "$(find . -name include -type d) "
          "$(find $libdirs -name '*.[ahco]' -o -name '*.prl' 2>/dev/null); do\n"
          "  if [ -e \"$pkgdir/$i\" ] || [ -L \"$pkgdir/$i\" ]; then\n"
          "    dest=\"$subpkgdir/${i%/*}\"\n"
          "    mkdir -p \"$dest\"\n"
          "    mv \"$pkgdir/$i\" \"$dest\"\n"
          "    rmdir \"$pkgdir/${i%/*}\" 2>/dev/null || :\n"
          "  fi\n"
          "done\n"
          "for i in lib/*.so usr/lib/*.so; do\n"
          "  if [ -L \"$i\" ]; then\n"
          "    mkdir -p \"$subpkgdir/${i%/*}\"\n"
          "    mv \"$i\" \"$subpkgdir/$i\" || return 1\n"
          "  fi\n"
          "done\n"
          "return 0\n");

    //
    // If any files were added, create the package.
    //

    if ((os.listdir)(build.vars.subpkgdir).length() != 0) {
        return true;
    }

    return false;
}

function
defaultPackagelang (
    build
    )

/*++

Routine Description:

    This routine implements the default lang subpackage function, which pulls
    out language files.

Arguments:

    build - Supplies the build manager object.

Return Value:

    Returns true if the package has contents and should be created.

    Returns false if it turns out there's no need to create the package.

--*/

{

    build.vars.description = "Languages for package " + build.vars.name;
    build.vars.section = "lang";
    shell("for d in ${langdir:-/usr/share/locale}; do\n"
          "  mkdir -p \"$subpkgdir\"/${d%/*}\n"
          "  mv \"$pkgdir\"/\"$d\" \"$subpkgdir\"/\"$d\"\n"
          "done");

    return true;
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

    (os.chdir)(vars.startdir);
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


/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Chalk

Abstract:

    This directory builds the Chalk modules.

Author:

    Evan Green 14-Feb-2017

Environment:

    C

--*/

from menv import copy, group, mconfig, sharedLibrary;

//
// Define the destination library paths, which when binplaced are relative to
// outroot.
//

var libPath = "bin/apps/usr/libl/chalk1";
var buildLibPath = "tools/lib/chalk1";

function chalkSharedModule(params) {

    //
    // Add the Chalk library dependency, and figure out the destination copy
    // path.
    //

    if (params.get("build")) {
        params.inputs.append("apps/ck/lib:build_libchalk_dynamic");
        params.binplace = buildLibPath;

    } else {
        params.inputs.append("apps/ck/lib:libchalk_dynamic");
        params.binplace = libPath;
    }

    params.nostrip = true;
    return sharedLibrary(params);
}

function build() {
    var buildAll = [];
    var all = [];
    var chalkModules;
    var entries = [];
    var foreignModules;

    chalkModules = [
        "bufferedio.ck",
        "cpio.ck",
        "fileio.ck",
        "getopt.ck",
        "io.ck",
        "iobase.ck",
        "lzfile.ck",
        "time.ck"
    ];

    foreignModules = [
        "_time",
        "app",
        "bundle",
        "json",
        "lzma",
        "os",
        "spawn",
    ];

    //
    // The chalk modules are just a straight copy.
    //

    for (module in chalkModules) {
        entries += copy(module,
                        mconfig.outroot + "/" + libPath + "/" + module,
                        module,
                        null,
                        null);

        entries += copy(module,
                        mconfig.outroot + "/" + buildLibPath + "/" + module,
                        "build_" + module,
                        null,
                        null);

        all.append(":" + module);
        buildAll.append(":build_" + module);
    }

    //
    // Add all the foreign modules to the all group.
    //

    for (module in foreignModules) {
        all.append("apps/ck/modules/" + module + ":all");
        buildAll.append("apps/ck/modules/" + module + ":build_all");
    }

    entries += group("modules", all);
    entries += group("build_modules", buildAll);
    return entries;
}


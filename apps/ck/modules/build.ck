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

var libPath = mconfig.binroot + "/chalk1/";
var buildLibPath = mconfig.outroot + "/tools/lib/chalk1/";

function chalkSharedModule(params) {
    var destinationPath;
    var newLabel;
    var result;

    //
    // Add the Chalk library dependency, and figure out the destination copy
    // path.
    //

    if (params.get("build")) {
        params.inputs.append("apps/ck/lib:build_libchalk_dynamic");
        destinationPath = buildLibPath;

    } else {
        params.inputs.append("apps/ck/lib:libchalk_dynamic");
        destinationPath = libPath;
    }

    params.binplace = false;
    result = sharedLibrary(params);

    //
    // Copy the file to the destination, then rename the label so that the copy
    // result gets linked into the dependency chain.
    //

    newLabel = params.label + "_orig";
    result += copy(":" + newLabel,
                   destinationPath + params.output,
                   params.label,
                   null,
                   null);

    params.label = newLabel;
    return result;
}

function build() {
    var all = [];
    var chalkModules;
    var entries = [];
    var foreignModules;

    chalkModules = [
        "bufferedio.ck",
        "fileio.ck",
        "getopt.ck",
        "io.ck",
        "iobase.ck"
    ];

    foreignModules = [
        "app",
        "bundle",
        "os"
    ];

    //
    // The chalk modules are just a straight copy.
    //

    for (module in chalkModules) {
        entries += copy(module, libPath + module, module, null, null);
        entries += copy(module,
                        buildLibPath + module,
                        "build_" + module,
                        null,
                        null);

        all.append(":" + module);
        all.append(":build_" + module);
    }

    //
    // Add all the foreign modules to the all group.
    //

    for (module in foreignModules) {
        all.append("apps/ck/modules/" + module + ":all");
    }

    entries += group("modules", all);
    return entries;
}


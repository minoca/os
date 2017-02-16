/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Chalk

Abstract:

    This executable implements the Chalk interactive interpreter.

Author:

    Evan Green 14-Feb-2017

Environment:

    C

--*/

from menv import application, mconfig;

function build() {
    var app;
    var buildLibs;
    var buildOs = mconfig.build_os;
    var buildSources;
    var commonSources;
    var entries;
    var libs;
    var sources;

    commonSources = [
        "chalk.c",
    ];

    sources = commonSources + [
        "ckunix.c"
    ];

    buildSources = sources;
    if (buildOs == "Windows") {
        buildSources = commonSources + [
            "ckwin32.c"
        ];
    }

    libs = [
        "apps/ck/lib:libchalk_dynamic",
        "apps/ck/modules/app:app",
        "apps/ck/modules/bundle:bundle"
    ];

    buildLibs = [
        "apps/ck/lib:build_libchalk_dynamic",
        "apps/ck/modules/app:build_app",
        "apps/ck/modules/bundle:build_bundle"
    ];

    app = {
        "label": "chalk",
        "inputs": sources + libs
    };

    entries = application(app);
    app = {
        "label": "build_chalk",
        "output": "chalk",
        "inputs": buildSources + buildLibs,
        "build": true,
        "prefix": "build",
        "binplace": "tools/bin"
    };

    entries += application(app);
    return entries;
}


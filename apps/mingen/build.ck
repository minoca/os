/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mingen

Abstract:

    This executable implements the Minoca build generator application, which
    converts build.ck files into Makefiles or Ninja files.

Author:

    Evan Green 5-Mar-2015

Environment:

    User

--*/

from menv import application, mconfig;

function build() {
    var app;
    var appName = "mingen";
    var bootstrapStamp;
    var bootstrapTool;
    var config;
    var entries;
    var includes;
    var libs;
    var sources;
    var tool;

    sources = [
        "make.ck",
        "mingen.ck",
        "ninja.ck",
        "bootstrap/remakebs.sh"
    ];

    if (mconfig.build_os == "Windows") {
        appName += ".exe";
    }

    config = {
        "CHALK_ARGS": "$IN $OUT"
    };

    app = {
        "type": "target",
        "label": "build_mingen",
        "output": appName,
        "inputs": ["mkbundle.ck"],
        "implicit": sources + ["apps/ck:build_chalk"],
        "tool": "create_mingen",
        "config": config
    };

    tool = {
        "type": "tool",
        "name": "create_mingen",
        "command": "chalk $in $out",
        "description": "Bundling mingen - $OUT"
    };

    //
    // If mingen ever gets rebuilt, also rebuild the bootstrap files in case
    // the recipe to make mingen changed.
    //

    bootstrapStamp = {
        "type": "target",
        "label": "bootstrap_stamp",
        "inputs": [":build_mingen"],
        "tool": "rebuild_bootstrap"
    };

    bootstrapTool = {
        "type": "tool",
        "name": "rebuild_bootstrap",
        "command": "$SHELL $S/apps/mingen/bootstrap/remakebs.sh $OUT",
        "description": "Rebuilding bootstrap Makefiles"
    };

    entries = [app, bootstrapStamp, tool, bootstrapTool];
    return entries;
}


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
    var config;
    var entries;
    var includes;
    var libs;
    var sources;
    var tool;

    sources = [
        "make.ck",
        "mingen.ck",
        "ninja.ck"
    ];

    if (mconfig.build_os == "win32") {
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
        "implicit": sources + ["apps/ck/app:build_chalk"],
        "tool": "create_mingen",
        "config": config
    };

    tool = {
        "type": "tool",
        "name": "create_mingen",
        "command": "chalk $in $out",
        "description": "Bundling mingen - $OUT"
    };

    entries = [app, tool];
    return entries;
}


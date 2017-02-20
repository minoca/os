/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    YyTest

Abstract:

    This program compiles the Lexer/Parser Library into an application and
    tests it.

Author:

    Evan Green 9-Oct-2015

Environment:

    Test

--*/

from menv import application;

function build() {
    var buildApp;
    var buildLibs;
    var entries;
    var sources;

    sources = [
        "yytest.c"
    ];

    buildLibs = [
        "lib/yy:build_yy"
    ];

    buildApp = {
        "label": "build_yytest",
        "output": "yytest",
        "inputs": sources + buildLibs,
        "build": true,
        "prefix": "build"
    };

    entries = application(buildApp);
    return entries;
}


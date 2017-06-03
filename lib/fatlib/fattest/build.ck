/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    FAT Test

Abstract:

    This program tests the FAT file system library in an application.

Author:

    Evan Green 9-Oct-2012

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
        "fatdev.c",
        "fattest.c"
    ];

    buildLibs = [
        "lib/fatlib:build_fat",
        "lib/rtl/base:build_basertl",
        "lib/rtl/urtl:build_rtlc"
    ];

    buildApp = {
        "label": "build_fattest",
        "output": "fattest",
        "inputs": sources + buildLibs,
        "build": true,
        "prefix": "build"
    };

    entries = application(buildApp);
    return entries;
}


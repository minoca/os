/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    STAB Test

Abstract:

    This program is used to test the symbol parser of the debug client.

Author:

    Evan Green 26-Jul-2012

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
        "teststab.c",
        "apps/debug/client:build/stabs.o",
        "apps/debug/client:build/coff.o",
        "apps/debug/client:build/elf.o",
        "apps/debug/client:build/symbols.o"
    ];

    buildLibs = [
        "lib/im:build_imu",
        "lib/rtl/base:build_basertl",
        "lib/rtl/urtl:build_rtlc",
    ];

    buildApp = {
        "label": "build_teststab",
        "output": "teststab",
        "inputs": sources + buildLibs,
        "build": true
    };

    entries = application(buildApp);
    return entries;
}


/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Disassembler Test

Abstract:

    This program is used to test the debugger's disassembler.

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
        "testdisa.c",
        "apps/debug/client:build/x86dis.o",
        "apps/debug/client:build/armdis.o",
        "apps/debug/client:build/disasm.o",
        "apps/debug/client:build/thmdis.o",
        "apps/debug/client:build/thm32dis.o",
    ];

    buildLibs = [
        "lib/im:build_imu",
        "lib/rtl/base:build_basertl",
        "lib/rtl/urtl:build_rtlc",
    ];

    buildApp = {
        "label": "build_testdisa",
        "output": "testdisa",
        "inputs": sources + buildLibs,
        "build": true
    };

    entries = application(buildApp);
    return entries;
}


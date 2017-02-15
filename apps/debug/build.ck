/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Debug

Abstract:

    This directory builds the debugger application and supporting extensions.

Author:

    Evan Green 26-Jul-2012

Environment:

    Debug

--*/

from menv import group, mconfig;

function build() {
    var buildOs = mconfig.build_os;
    var debugBinaries;
    var entries;

    debugBinaries = [
        "apps/debug/client:debug",
        "apps/debug/kexts:kexts",
        "apps/debug/kexts:build_kexts"
    ];

    if ((buildOs == "Windows") || (buildOs == "Minoca")) {
        debugBinaries += [
            "apps/debug/client:build_debug",
            "apps/debug/client/tdwarf:build_tdwarf",
            "apps/debug/client/testdisa:build_testdisa",
            "apps/debug/client/teststab:build_teststab",
        ];
    }

    if (buildOs == "Windows") {
        debugBinaries += [
            "apps/debug/client:build_debugui"
        ];
    }

    entries = group("debug", debugBinaries);
    return entries;
}


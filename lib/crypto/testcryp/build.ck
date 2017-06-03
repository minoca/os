/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    TestCryp

Abstract:

    This program compiles the Cryptographic Library into an application and
    tests it.

Author:

    Evan Green 14-Jan-2015

Environment:

    Test

--*/

from menv import application;

function build() {
    var buildLibs;
    var buildApp;
    var entries;
    var sources;

    sources = [
        "testcryp.c"
    ];

    buildLibs = [
        "lib/crypto/ssl:build_ssl",
        "lib/crypto:build_crypto",
        "lib/rtl/urtl:build_rtlc",
        "lib/rtl/base:build_basertl"
    ];

    buildApp = {
        "label": "build_testcryp",
        "output": "testcryp",
        "inputs": sources + buildLibs,
        "build": true,
        "prefix": "build"
    };

    entries = application(buildApp);
    return entries;
}


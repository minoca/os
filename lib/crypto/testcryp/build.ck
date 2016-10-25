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

function build() {
    sources = [
        "testcryp.c"
    ];

    build_libs = [
        "//lib/crypto/ssl:build_ssl",
        "//lib/crypto:build_crypto",
        "//lib/rtl/rtlc:build_rtlc",
        "//lib/rtl/base:build_basertl"
    ];

    build_app = {
        "label": "build_testcryp",
        "output": "testcryp",
        "inputs": sources + build_libs,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(build_app);
    return entries;
}

return build();


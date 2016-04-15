/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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


/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    C Library Test

Abstract:

    This program tests the independent portions of the C library.

Author:

    Evan Green 9-Jul-2013

Environment:

    Test

--*/

function build() {
    sources = [
        "bsrchtst.c",
        "getoptst.c",
        "mathtst.c",
        "qsorttst.c",
        "regextst.c",
        "testc.c"
    ];

    build_libs = [
        "//apps/libc/dynamic:build_libc",
    ];

    sources_cppflags = [
        "$BUILD_CPPFLAGS",
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "BUILD_CPPFLAGS": sources_cppflags
    };

    build_app = {
        "label": "build_testc",
        "output": "testc",
        "inputs": sources + build_libs,
        "sources_config": sources_config,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(build_app);
    return entries;
}

return build();


/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    mingen

Abstract:

    This module implements support for the Minoca build generator.

Author:

    Evan Green 3-Dec-2015

Environment:

    Any

--*/

function build() {
    base_sources = [
        "chkfuncs.c",
        "make.c",
        "mingen.c",
        "ninja.c",
        "path.c",
        "script.c"
    ];

    uos_sources = [
        "uos.c"
    ];

    win32_sources = [
        "ntos.c"
    ];

    sources = base_sources + uos_sources;
    build_sources = sources;
    if (build_os == "Windows") {
        build_sources = base_sources + win32_sources;
    }

    libs = [
        "//apps/lib/chalk:chalk",
        "//lib/yy:yy",
        "//lib/rtl/rtlc:rtlc",
        "//lib/rtl/base/:basertl"
    ];

    build_libs = [
        "//apps/lib/chalk:build_chalk",
        "//lib/yy:build_yy",
        "//lib/rtl/rtlc:build_rtlc",
        "//lib/rtl/base:build_basertl"
    ];

    includes = [
        "-I$///apps/include",
        "-I$///apps/lib/chalk"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS", "-I$///apps/include/libc"] + includes
    };

    build_sources_config = {
        "BUILD_CPPFLAGS": ["$BUILD_CPPFLAGS"] + includes
    };

    app = {
        "label": "mingen",
        "inputs": sources + libs,
        "sources_config": sources_config
    };

    build_app = {
        "label": "mingen_build",
        "output": "mingen",
        "inputs": build_sources + build_libs,
        "sources_config": build_sources_config,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(app);
    entries += application(build_app);
    return entries;
}

return build();


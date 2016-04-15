/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    msetup

Abstract:

    This executable implements the setup (OS installer) executable.

Author:

    Evan Green 10-Apr-2014

Environment:

    User

--*/

function build() {
    common_sources = [
        "cache.c",
        "disk.c",
        "fatdev.c",
        "fileio.c",
        "indat.c",
        "partio.c",
        "plat.c",
        "setup.c",
        "steps.c",
        "util.c"
    ];

    minoca_sources = [
        "minoca/io.c",
        "minoca/misc.c",
        "minoca/part.c"
    ];

    uos_sources = [
        "uos/io.c",
        "uos/misc.c",
        "uos/part.c"
    ];

    win32_sources = [
        "win32/io.c",
        "win32/misc.c",
        "win32/msetuprc.rc",
        "win32/part.c",
        "win32/win32sup.c"
    ];

    target_libs = [
        "//lib/partlib:partlib",
        "//lib/fatlib:fat",
        "//lib/bconflib:bconf",
        "//lib/rtl/base:basertl",
        "//apps/osbase/urtl:urtl",
        "//apps/lib/chalk:chalk",
        "//lib/yy:yy",
        "//apps/setup/config:msetplat"
    ];

    build_libs = [
        "//lib/partlib:build_partlib",
        "//lib/fatlib:build_fat",
        "//lib/bconflib:build_bconf",
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc",
        "//apps/lib/chalk:build_chalk",
        "//lib/yy:build_yy",
        "//apps/setup/config:build_msetplat"
    ];

    target_dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    target_includes = [
        "-I$///apps/include",
        "-I$///apps/include/libc",
        "-I$///apps/lib/chalk"
    ];

    build_includes = [
        "-I$///apps/include",
        "-I$///apps/lib/chalk"
    ];

    target_sources = common_sources + minoca_sources + target_libs +
                     target_dynlibs;

    build_config = {};
    if (build_os == "Windows") {
        build_sources = common_sources + win32_sources;
        build_config["DYNLIBS"] = ["-lsetupapi"];

    } else if (build_os == "Minoca") {
        build_sources = common_sources + minoca_sources + target_dynlibs;
        build_includes = target_includes;

    } else {
        build_sources = common_sources + uos_sources;
    }

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + target_includes
    };

    build_sources_config = {
        "BUILD_CPPFLAGS": ["$BUILD_CPPFLAGS"] + build_includes
    };

    app = {
        "label": "msetup",
        "inputs": target_sources,
        "sources_config": sources_config
    };

    build_app = {
        "label": "build_msetup",
        "output": "msetup",
        "inputs": build_sources + build_libs,
        "sources_config": build_sources_config,
        "config": build_config,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(app);
    entries += application(build_app);
    return entries;
}

return build();

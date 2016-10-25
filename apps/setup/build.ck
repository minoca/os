/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
        "config.S",
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
        "//lib/yy:yy"
    ];

    build_libs = [
        "//lib/partlib:build_partlib",
        "//lib/fatlib:build_fat",
        "//lib/bconflib:build_bconf",
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc",
        "//apps/lib/chalk:build_chalk",
        "//lib/yy:build_yy"
    ];

    target_dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    build_includes = [
        "$//apps/lib/chalk",
        "$//apps/setup/config"
    ];

    target_includes = build_includes + [
        "$//apps/libc/include",
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

    app = {
        "label": "msetup",
        "inputs": target_sources,
        "includes": target_includes
    };

    build_app = {
        "label": "build_msetup",
        "output": "msetup",
        "inputs": build_sources + build_libs,
        "includes": build_includes,
        "config": build_config,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(app);
    entries += application(build_app);
    setup_tool = {
        "type": "tool",
        "name": "msetup_image",
        "command": "$^/apps/setup/build/msetup $MSETUP_FLAGS -d $OUT",
        "description": "Building Image - $OUT",
        "pool": "image"
    };

    image_pool = {
        "type": "pool",
        "name": "image",
        "depth": 1
    };

    entries += [setup_tool, image_pool];
    return entries;
}

return build();

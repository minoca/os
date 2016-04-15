/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Setup Configuration Files

Abstract:

    This directory builds setup recipes into object files.

Author:

    Evan Green 21-Oct-2015

Environment:

    Setup

--*/

function build() {
    sources = [
        "bbone.txt",
        "common.txt",
        "galileo.txt",
        "instarv6.txt",
        "instarv7.txt",
        "instx86.txt",
        "integrd.txt",
        "panda.txt",
        "pandausb.txt",
        "pc.txt",
        "pcefi.txt",
        "pctiny.txt",
        "rpi.txt",
        "rpi2.txt",
        "veyron.txt"
    ];

    lib = {
        "label": "msetplat",
        "inputs": sources
    };

    build_lib = {
        "label": "build_msetplat",
        "output": "msetplat",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = objectified_library(lib);
    entries += objectified_library(build_lib);
    return entries;
}

return build();

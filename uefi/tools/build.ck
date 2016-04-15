/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    UEFI Tools

Abstract:

    This directory compiles build-time tools used to generate final UEFI
    images.

Author:

    Evan Green 6-Mar-2014

Environment:

    Build

--*/

function build() {
    tool_names = [
        "elfconv",
        "genffs",
        "genfv",
        "mkuboot",
    ];

    apps = [];
    for (tool_name in tool_names) {
        apps += ["//uefi/tools/" + tool_name + ":" + tool_name];
    }

    uefitools_group = group("uefitools", apps);
    return uefitools_group;
}

return build();

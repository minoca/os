/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

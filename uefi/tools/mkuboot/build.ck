/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    mkuboot

Abstract:

    This module builds the mkuboot build utility, which can create a U-Boot
    firmware images.

Author:

    Chris Stevens 2-Jul-2015

Environment:

    Build

--*/

function build() {
    sources = [
        "mkuboot.c",
    ];

    libs = [
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc"
    ];

    includes = [
        "-I$///uefi/include"
    ];

    sources_config = {
        "BUILD_CPPFLAGS": ["$BUILD_CPPFLAGS"] + includes
    };

    app = {
        "label": "mkuboot",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "build": TRUE
    };

    entries = application(app);
    mkuboot_command = "$^//uefi/tools/mkuboot/mkuboot $MKUBOOT_FLAGS " +
                      "-l $TEXT_ADDRESS -e $TEXT_ADDRESS -o $OUT $IN";

    tool = {
        "type": "tool",
        "name": "mkuboot",
        "command": mkuboot_command,
        "description": "Creating U-Boot Image - $OUT"
    };

    return entries + [tool];
}

return build();

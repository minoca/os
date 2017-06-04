/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    BeagleBone Stage 1 Loader

Abstract:

    This module implements Texas Instruments AM335x first stage loader.

Author:

    Evan Green 17-Dec-2014

Environment:

    Firmware

--*/

from menv import application, staticApplication, binplace, executable,
    flattenedBinary;

function build() {
    var bbonefwb;
    var bbonefwbTool;
    var builderApp;
    var builderSources;
    var elf;
    var entries;
    var flattened;
    var includes;
    var linkConfig;
    var sources;
    var textAddress = "0x402F0408";

    sources = [
        "armv7/start.S",
        "boot.c",
        "clock.c",
        "uefi/plat/panda/init:crc32.o",
        "uefi/plat/panda/init:fatboot.o",
        "mux.c",
        "power.c",
        "serial.c",
        "uefi/plat/panda/init:rommem.o"
    ];

    includes = [
        "$S/uefi/include",
        "$S/uefi/plat/panda/init"
    ];

    linkConfig = {
        "LDFLAGS": ["-Wl,-zmax-page-size=1"]
    };

    elf = {
        "label": "bbonemlo.elf",
        "inputs": sources,
        "includes": includes,
        "text_address": textAddress,
        "config": linkConfig
    };

    entries = staticApplication(elf);

    //
    // Flatten the firmware image and add the TI header.
    //

    flattened = {
        "label": "bbonemlo.bin",
        "inputs": [":bbonemlo.elf"]
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    bbonefwbTool = {
        "type": "tool",
        "name": "bbonefwb",
        "command": "$O/uefi/plat/beagbone/init/fwbuild $text_address $IN $OUT",
        "description": "Building BeagleBone Firmware - $OUT"
    };

    entries += [bbonefwbTool];
    bbonefwb = {
        "type": "target",
        "label": "bbonemlo",
        "tool": "bbonefwb",
        "inputs": [":bbonemlo.bin"],
        "implicit": [":fwbuild"],
        "config": {"text_address": textAddress},
        "nostrip": true
    };

    entries += binplace(bbonefwb);

    //
    // Add the firmware builder tool.
    //

    builderSources = [
        "bbonefwb/fwbuild.c"
    ];

    builderApp = {
        "label": "fwbuild",
        "inputs": builderSources,
        "build": true
    };

    entries += application(builderApp);
    return entries;
}


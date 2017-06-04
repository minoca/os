/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    PandaBoard Stage 1 Loader

Abstract:

    This module implements Texas Instruments OMAP4 first stage loader.

Author:

    Evan Green 1-Apr2014

Environment:

    Firmware

--*/

from menv import application, staticApplication, binplace, executable,
    flattenedBinary;

function build() {
    var elf;
    var entries;
    var builderApp;
    var builderSources;
    var flattened;
    var fwbTool;
    var includes;
    var mlo;
    var sources;
    var sourcesConfig;
    var textAddress = "0x40308000";

    sources = [
        "armv7/start.S",
        "boot.c",
        "clock.c",
        "crc32.c",
        "fatboot.c",
        "gpio.c",
        "id.c",
        "mux.c",
        "serial.c",
        "rommem.c",
        "romusb.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-marm"]
    };

    elf = {
        "label": "omap4mlo.elf",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes,
        "text_address": textAddress,
    };

    entries = staticApplication(elf);

    //
    // Flatten the firmware image and add the TI header.
    //

    flattened = {
        "label": "omap4mlo.bin",
        "inputs": [":omap4mlo.elf"]
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    fwbTool = {
        "type": "tool",
        "name": "pandafwb",
        "command": "$O/uefi/plat/panda/init/pandafwb $text_address $IN $OUT",
        "description": "Building PandaBoard Firmware - $OUT"
    };

    entries += [fwbTool];
    mlo = {
        "type": "target",
        "label": "omap4mlo",
        "tool": "pandafwb",
        "inputs": [":omap4mlo.bin"],
        "implicit": [":pandafwb"],
        "config": {"text_address": "0x40300000"},
        "nostrip": true
    };

    entries += binplace(mlo);

    //
    // Add the firmware builder tool.
    //

    builderSources = [
        "fwbuild/fwbuild.c"
    ];

    builderApp = {
        "label": "pandafwb",
        "inputs": builderSources,
        "build": true
    };

    entries += application(builderApp);
    return entries;
}


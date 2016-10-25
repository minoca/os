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

function build() {
    text_address = "0x40308000";
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
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-marm"]
    };

    link_ldflags = [
        "-nostdlib",
        "-static"
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

    elf = {
        "label": "omap4mlo.elf",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes,
        "linker_script": "$//uefi/plat/panda/init/link.x",
        "text_address": text_address,
        "config": link_config
    };

    entries = executable(elf);

    //
    // Flatten the firmware image and add the TI header.
    //

    flattened = {
        "label": "omap4mlo.bin",
        "inputs": [":omap4mlo.elf"]
    };

    flattened = flattened_binary(flattened);
    entries += flattened;
    fwb_tool = {
        "type": "tool",
        "name": "pandafwb",
        "command": "$^//uefi/plat/panda/init/pandafwb $TEXT_ADDRESS $IN $OUT",
        "description": "Building PandaBoard Firmware - $OUT"
    };

    entries += [fwb_tool];
    mlo = {
        "type": "target",
        "label": "omap4mlo",
        "tool": "pandafwb",
        "inputs": [":omap4mlo.bin"],
        "implicit": [":pandafwb"],
        "config": {"TEXT_ADDRESS": "0x40300000"},
        "nostrip": TRUE
    };

    entries += binplace(mlo);

    //
    // Add the firmware builder tool.
    //

    builder_sources = [
        "fwbuild/fwbuild.c"
    ];

    builder_app = {
        "label": "pandafwb",
        "inputs": builder_sources,
        "build": TRUE
    };

    entries += application(builder_app);
    return entries;
}

return build();

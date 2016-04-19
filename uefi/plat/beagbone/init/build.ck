/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    BeagleBone Stage 1 Loader

Abstract:

    This module implements Texas Instruments AM335x first stage loader.

Author:

    Evan Green 17-Dec-2014

Environment:

    Firmware

--*/

function build() {
    text_address = "0x402F0408";
    sources = [
        "armv7/start.S",
        "boot.c",
        "clock.c",
        "//uefi/plat/panda/init:crc32.o",
        "//uefi/plat/panda/init:fatboot.o",
        "mux.c",
        "power.c",
        "serial.c",
        "//uefi/plat/panda/init:rommem.o"
    ];

    includes = [
        "$//uefi/include",
        "$//uefi/plat/panda/init"
    ];

    link_ldflags = [
        "-nostdlib",
        "-static"
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

    elf = {
        "label": "bbonemlo.elf",
        "inputs": sources,
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
        "label": "bbonemlo.bin",
        "inputs": [":bbonemlo.elf"]
    };

    flattened = flattened_binary(flattened);
    entries += flattened;
    bbonefwb_tool = {
        "type": "tool",
        "name": "bbonefwb",
        "command": "$^//uefi/plat/beagbone/init/fwbuild $TEXT_ADDRESS $IN $OUT",
        "description": "Building BeagleBone Firmware - $OUT"
    };

    bbonefwb = {
        "type": "target",
        "label": "bbonemlo",
        "tool": "bbonefwb",
        "inputs": [":bbonemlo.bin"],
        "implicit": [":fwbuild"],
        "config": {"TEXT_ADDRESS": text_address}
    };

    entries += [bbonefwb_tool, bbonefwb];

    //
    // Add the firmware builder tool.
    //

    builder_sources = [
        "bbonefwb/fwbuild.c"
    ];

    builder_app = {
        "label": "fwbuild",
        "inputs": builder_sources,
        "build": TRUE
    };

    entries += application(builder_app);
    return entries;
}

return build();

/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    BCM2709 UEFI Device Library

Abstract:

    This library contains support for the BCM2709 SoC's devices.

Author:

    Chris Stevens 18-Mar-2015

Environment:

    Firmware

--*/

function build() {
    sources = [
        "gpio.c",
        "init.c",
        "intr.c",
        "mailbox.c",
        "memmap.c",
        "sd.c",
        "serial.c",
        "timer.c",
        "usb.c",
        "video.c",
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "bcm2709",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();

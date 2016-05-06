/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    UEFI

Abstract:

    This directory builds UEFI firmware images for several platforms.

Author:

    Evan Green 26-Feb-2014

Environment:

    Firmware

--*/

function build() {
    if (arch == "armv7") {
        platfw = [
            "//uefi/plat/beagbone:bbonefw",
            "//uefi/plat/beagbone/init:bbonemlo",
            "//uefi/plat/panda/init:omap4mlo",
            "//uefi/plat/panda:pandafw",
            "//uefi/plat/rpi2:rpi2fw",
            "//uefi/plat/veyron:veyronfw",
        ];

    } else if (arch == "armv6") {
        platfw = [
            "//uefi/plat/rpi:rpifw"
        ];

    } else if (arch == "x86") {
        platfw = [
            "//uefi/plat/bios:biosfw",
        ];
    }

    entries = group("platfw", platfw);
    return entries;
}

return build();

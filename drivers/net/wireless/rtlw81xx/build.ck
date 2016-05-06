/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    RTLW 81xx

Abstract:

    This module implements support for the Realtek RTL81xx family of
    wireless LAN controllers.

Author:

    Chris Stevens 10-Oct-2015

Environment:

    Kernel

--*/

function build() {
    name = "rtlw81xx";
    sources = [
        "rtlw81.c",
        "rtlw81hw.c"
    ];

    dynlibs = [
        "//drivers/net/netcore:netcore",
        "//drivers/net/net80211:net80211",
        "//drivers/usb/usbcore:usbcore"
    ];

    fw_files = [
        "rtlw8188cufwUMC.bin",
        "rtlw8188eufw.bin",
        "rtlw8192cufw.bin"
    ];

    entries = [];
    implicits = [];
    for (fw_file in fw_files) {
        entries += copy("firmware/" + fw_file,
                        binroot + "/" + fw_file,
                        fw_file,
                        null,
                        null);

        implicits += [":" + fw_file];
    }

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
        "implicit": implicits
    };

    entries += driver(drv);
    return entries;
}

return build();

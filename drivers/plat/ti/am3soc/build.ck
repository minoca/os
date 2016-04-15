/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    AM335x SoC

Abstract:

    This module implements the TI AM335x SoC support driver.

Author:

    Evan Green 9-Sep-2015

Environment:

    Kernel

--*/

function build() {
    name = "am3soc";
    fw_name = "am3cm3fw";
    sources = [
        "am3soc.c",
        ":" + fw_name + ".o",
        "mailbox.c",
        "sleep.S"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);

    //
    // Objectify the firmware into the driver.
    //

    fw_o = {
        "label": fw_name + ".o",
        "inputs": [fw_name + ".bin"]
    };

    entries += objectified_binary(fw_o);
    return entries;
}

return build();

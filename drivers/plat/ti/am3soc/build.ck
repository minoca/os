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
        "am3cm3fw.S",
        "mailbox.c",
        "sleep.S"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();

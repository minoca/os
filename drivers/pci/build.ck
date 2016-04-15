/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    PCI

Abstract:

    This module implements the Peripheral Component Interconnect (PCI)
    driver.

Author:

    Evan Green 16-Sep-2012

Environment:

    Kernel

--*/

function build() {
    name = "pci";
    sources = [
        "msi.c",
        "pci.c",
        "rootbus.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();

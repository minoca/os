/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "pci";
    var sources;

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


/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    EDMA3

Abstract:

    This module implements support for the TI EDMA 3 controller.

Author:

    Evan Green 1-Feb-2016

Environment:

    Kernel

--*/

function build() {
    name = "edma3";
    sources = [
        "edma3.c"
    ];

    dynlibs = [
        "//drivers/dma/core:dma"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();

/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    BCM2709 DMA

Abstract:

    This module implements support for the Broadcom 2709 DMA controller.

Author:

    Chris Stevens 12-Feb-2016

Environment:

    Kernel

--*/

function build() {
    name = "dmab2709";
    sources = [
        "dmab2709.c"
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

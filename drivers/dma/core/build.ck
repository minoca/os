/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    DMA Core

Abstract:

    This module implements the DMA core support library. It provides
    generic support infrastructure for all DMA controllers.

Author:

    Evan Green 1-Feb-2016

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "dma";
    var sources;

    sources = [
        "dma.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}


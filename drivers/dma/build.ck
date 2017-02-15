/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    DMA

Abstract:

    This directory builds system DMA drivers.

Author:

    Evan Green 1-Feb-2016

Environment:

    Kernel

--*/

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var dmaDrivers;
    var entries;

    if (!(["armv7", "armv6"].contains(arch))) {
        Core.raise(ValueError("Unexpected Architecture"));
    }

    dmaDrivers = [
        "drivers/dma/bcm2709:dmab2709",
        "drivers/dma/edma3:edma3",
    ];

    entries = group("dma_drivers", dmaDrivers);
    return entries;
}


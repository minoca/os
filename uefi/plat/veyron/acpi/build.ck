/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Veyron ACPI tables

Abstract:

    This module compiles the Veyron ACPI tables.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

from menv import compiledAsl;

function build() {
    var asl;
    var entries;
    var ffs;
    var ffsSources;
    var sources;

    sources = [
        "apic.asl",
        "dbg2.asl",
        "dsdt.asl",
        "facp.asl",
        "facs.asl",
        "gtdt.asl",
        "rk32.asl"
    ];

    asl = compiledAsl(sources);
    entries = asl[1];
    ffsSources = asl[0];
    ffs = {
        "type": "target",
        "label": "acpi.ffs",
        "inputs": ffsSources,
        "implicit": ["uefi/tools/genffs:genffs"],
        "tool": "genffs_acpi"
    };

    entries += [ffs];
    return entries;
}


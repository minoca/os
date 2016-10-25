/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Raspberry Pi 2 ACPI Tables

Abstract:

    This module compiles the Raspberry Pi 2 ACPI tables.

Author:

    Chris Stevens 17-Mar-2015

Environment:

    Firmware

--*/

function build() {
    sources = [
        "bcm2.asl",
        "dbg2.asl",
        "dsdt.asl",
        "facp.asl",
        "facs.asl",
        "gtdt.asl"
    ];

    asl = compiled_asl(sources);
    entries = asl[1];
    ffs_sources = asl[0];
    ffs = {
        "label": "acpi.ffs",
        "inputs": ffs_sources,
        "implicit": ["//uefi/tools/genffs:genffs"],
        "tool": "genffs_acpi"
    };

    entries += [ffs];
    return entries;
}

return build();

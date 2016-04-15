/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Raspberry Pi ACPI Tables

Abstract:

    This module compiles the Raspberry Pi ACPI tables.

Author:

    Chris Stevens 31-Dec-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "bcm2.asl",
        "dbg2.asl",
        "dsdt.asl",
        "facp.asl",
        "facs.asl"
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
